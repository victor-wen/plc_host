#include "DashboardScene.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

#include "dashboard/commands/AddItemCommand.h"
#include "dashboard/commands/MoveCommand.h"
#include "dashboard/commands/RemoveItemCommand.h"
#include "dashboard/commands/ResizeCommand.h"
#include "dashboard/items/ImageItem.h"
#include "dashboard/items/LedItem.h"
#include "dashboard/items/RectItem.h"
#include "dashboard/items/SwitchItem.h"
#include "dashboard/items/TextItem.h"
#include "dashboard/items/ValueItem.h"

#include <algorithm>

namespace {

// 已冻结的组件类型清单 (docs/architecture/interfaces.md §9)。
const QStringList& knownItemTypes()
{
    static const QStringList types = {
        QStringLiteral("text"),       QStringLiteral("rect"),
        QStringLiteral("image"),      QStringLiteral("value"),
        QStringLiteral("led"),        QStringLiteral("switch"),
        QStringLiteral("progress"),   QStringLiteral("gauge"),
        QStringLiteral("trend"),      QStringLiteral("button"),
    };
    return types;
}

// 占位组件：具体组件（DASH-05+）实现前，工厂用占位组件承载场景几何与元数据。
// 未知类型或显式 errorPlaceholder 渲染为黄色占位框（接口 §9 降级语义），
// 不阻塞其他组件/页面。
class PlaceholderItem : public DashboardBaseItem {
public:
    explicit PlaceholderItem(const QString& type, QGraphicsItem* parent = nullptr)
        : DashboardBaseItem(parent)
        , m_isError(type == QStringLiteral("errorPlaceholder")
                    || !knownItemTypes().contains(type))
    {
        itemType = type;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

private:
    bool m_isError = false;
};

void PlaceholderItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                            QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF rect = boundingRect();
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (m_isError) {
        painter->setBrush(QColor(0xFF, 0xE0, 0x5A)); // 黄色占位
        painter->setPen(QPen(QColor(0xB0, 0x8A, 0x00), 1, Qt::DashLine));
    } else {
        painter->setBrush(QColor(0x2A, 0x2E, 0x38));
        painter->setPen(QPen(QColor(0x5A, 0x64, 0x75), 1, Qt::DashLine));
    }
    painter->drawRoundedRect(rect, 4, 4);
    painter->setPen(QColor(0xE0, 0xE3, 0xEA));
    painter->drawText(rect, Qt::AlignCenter, itemType);
}

// background 为 #RRGGBB 颜色或图片资源路径；空解析失败 → 默认深色（Luna dark）。
QColor pageBackground(const QString& background)
{
    if (background.startsWith(QLatin1Char('#'))) {
        const QColor color(background);
        if (color.isValid())
            return color;
    }
    return QColor(0x1E, 0x21, 0x28);
}

} // namespace

DashboardScene::DashboardScene(QObject* parent)
    : QGraphicsScene(parent)
{
    // 组件数 <100：线性索引，添加/移动/删除为常数级开销。
    setItemIndexMethod(QGraphicsScene::NoIndex);
}

void DashboardScene::setPage(const DashboardPage& page)
{
    setSceneRect(0, 0, page.width, page.height);
    setBackgroundBrush(pageBackground(page.background));
}

DashboardBaseItem* DashboardScene::addItem(const DashboardItem& meta)
{
    // 工厂：DASH-05 起按 itemType 分发到具体组件；未知/损坏类型降级为
    // PlaceholderItem（黄色占位框，接口 §9 降级语义）。
    DashboardBaseItem* item = nullptr;
    if (meta.itemType == QStringLiteral("text"))
        item = new TextItem;
    else if (meta.itemType == QStringLiteral("rect"))
        item = new RectItem;
    else if (meta.itemType == QStringLiteral("image"))
        item = new ImageItem;
    else if (meta.itemType == QStringLiteral("value"))
        item = new ValueItem;
    else if (meta.itemType == QStringLiteral("led"))
        item = new LedItem;
    else if (meta.itemType == QStringLiteral("switch"))
        item = new SwitchItem;
    else
        item = new PlaceholderItem(meta.itemType);

    item->itemId = meta.id;
    item->itemType = meta.itemType;
    item->commonStyle = meta.commonStyle;
    item->config = meta.config;
    item->schemaVersion = meta.schemaVersion;
    item->setSize(meta.width, meta.height);
    item->setPos(meta.x, meta.y);
    item->setZValue(meta.zOrder);
    item->setEditMode(m_editMode);
    // 从 config 恢复组件业务属性（表现层属性读取已赋值的 commonStyle）。
    item->deserialize(meta.config);
    QGraphicsScene::addItem(item);
    return item;
}

void DashboardScene::setEditMode(bool editing)
{
    m_editMode = editing;
    const auto items = dashboardItems();
    for (auto* item : items)
        item->setEditMode(editing);
}

QList<DashboardBaseItem*> DashboardScene::dashboardItems() const
{
    QList<DashboardBaseItem*> result;
    const auto all = QGraphicsScene::items();
    result.reserve(all.size());
    for (QGraphicsItem* item : all) {
        if (auto* dbi = dynamic_cast<DashboardBaseItem*>(item))
            result.append(dbi);
    }
    return result;
}

QList<QGraphicsItem*> DashboardScene::topLevelItems() const
{
    QList<QGraphicsItem*> result;
    const auto all = QGraphicsScene::items();
    result.reserve(all.size());
    for (QGraphicsItem* item : all) {
        if (!item->parentItem())
            result.append(item);
    }
    return result;
}

QPointF DashboardScene::snapToGrid(QPointF pos, int gridSize) const
{
    if (gridSize <= 0)
        return pos;
    const qreal x = qRound(pos.x() / gridSize) * gridSize;
    const qreal y = qRound(pos.y() / gridSize) * gridSize;
    return QPointF(x, y);
}

void DashboardScene::bringToFront()
{
    auto sel = selectedItems();
    if (sel.isEmpty())
        return;

    qreal top = 0.0;
    for (auto* item : dashboardItems())
        top = qMax(top, item->zValue());

    // 保持选中集内部相对层级：QGraphicsScene::selectedItems() 的顺序无保证，
    // 显式按当前 z 升序后依次抬升（最上层获得最高 z）。
    std::sort(sel.begin(), sel.end(),
              [](DashboardBaseItem* lhs, DashboardBaseItem* rhs) {
                  return lhs->zValue() < rhs->zValue();
              });
    for (auto* item : sel) {
        item->setZValue(top + 1.0);
        top += 1.0;
    }
}

void DashboardScene::sendToBack()
{
    auto sel = selectedItems();
    if (sel.isEmpty())
        return;

    qreal bottom = 0.0;
    for (auto* item : dashboardItems())
        bottom = qMin(bottom, item->zValue());

    // 保持选中集内部相对层级：按当前 z 降序后依次下沉（最上层先下沉）。
    std::sort(sel.begin(), sel.end(),
              [](DashboardBaseItem* lhs, DashboardBaseItem* rhs) {
                  return lhs->zValue() > rhs->zValue();
              });
    for (auto* item : sel) {
        item->setZValue(bottom - 1.0);
        bottom -= 1.0;
    }
}

void DashboardScene::stepForward()
{
    for (auto* item : selectedItems())
        item->setZValue(item->zValue() + 1.0);
}

void DashboardScene::stepBackward()
{
    for (auto* item : selectedItems())
        item->setZValue(item->zValue() - 1.0);
}

void DashboardScene::copySelected()
{
    const auto sel = selectedItems();
    if (sel.isEmpty())
        return;

    QJsonArray array;
    for (auto* item : sel)
        array.append(dashboardItemToJson(item));
    m_clipboard = array;

    // 镜像到系统剪贴板（JSON 文本），便于外部粘贴。
    if (auto* clipboard = QApplication::clipboard())
        clipboard->setText(QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

void DashboardScene::pasteClipboard()
{
    if (m_clipboard.isEmpty())
        return;

    QList<DashboardBaseItem*> pasted;
    pasted.reserve(m_clipboard.size());
    for (const QJsonValue& value : m_clipboard) {
        DashboardItem meta = dashboardItemFromJson(value.toObject());
        meta.id = -1; // 粘贴产生新组件（未保存）
        meta.x += 20.0; // 相对原位置偏移 +20px
        meta.y += 20.0;
        pasted.append(addItem(meta));
    }

    // 粘贴后仅选中新组件。
    clearSelection();
    for (auto* item : pasted)
        item->setSelected(true);
}

void DashboardScene::keyPressEvent(QKeyEvent* event)
{
    if (m_editMode) {
        if (event->matches(QKeySequence::Copy)) {
            copySelected();
            event->accept();
            return;
        }
        if (event->matches(QKeySequence::Paste)) {
            pasteClipboard();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Delete) {
            deleteSelected();
            event->accept();
            return;
        }
    }
    QGraphicsScene::keyPressEvent(event);
}

void DashboardScene::alignSelected(Qt::Alignment alignment)
{
    const auto sel = selectedItems();
    if (sel.isEmpty())
        return;

    // 以选中集的包围盒作为对齐基准。
    QRectF bounds;
    for (auto* item : sel)
        bounds |= item->sceneBoundingRect();

    for (auto* item : sel) {
        const QRectF rect(item->pos(), item->boundingRect().size());
        QPointF pos = rect.topLeft();

        if (alignment.testFlag(Qt::AlignLeft))
            pos.setX(bounds.left());
        else if (alignment.testFlag(Qt::AlignHCenter))
            pos.setX(bounds.center().x() - rect.width() / 2.0);
        else if (alignment.testFlag(Qt::AlignRight))
            pos.setX(bounds.right() - rect.width());

        if (alignment.testFlag(Qt::AlignTop))
            pos.setY(bounds.top());
        else if (alignment.testFlag(Qt::AlignVCenter))
            pos.setY(bounds.center().y() - rect.height() / 2.0);
        else if (alignment.testFlag(Qt::AlignBottom))
            pos.setY(bounds.bottom() - rect.height());

        item->setPos(pos);
    }
}

void DashboardScene::deleteSelected()
{
    const auto sel = selectedItems();
    if (sel.isEmpty())
        return;

    // 批量删除作为一个宏命令：一次撤销恢复全部被删组件（QUndoCommand 子命令）。
    auto* macro = new QUndoCommand;
    macro->setText(QStringLiteral("删除 %1 个组件").arg(sel.size()));
    for (auto* item : sel) {
        item->setSelected(false);
        new RemoveItemCommand(this, item, dashboardItemToJson(item), macro);
    }
    m_undoStack.push(macro);
}

DashboardBaseItem* DashboardScene::addItemWithUndo(const DashboardItem& meta)
{
    auto* cmd = new AddItemCommand(this, meta);
    m_undoStack.push(cmd); // push 即执行 redo
    return cmd->item();
}

void DashboardScene::moveItem(DashboardBaseItem* item, const QPointF& newPos)
{
    if (!item || item->pos() == newPos)
        return;
    m_undoStack.push(new MoveCommand(item, item->pos(), newPos));
}

void DashboardScene::resizeItem(DashboardBaseItem* item, const QRectF& newRect)
{
    if (!item)
        return;
    const QRectF oldRect(item->pos(), item->boundingRect().size());
    if (oldRect == newRect)
        return;
    m_undoStack.push(new ResizeCommand(item, oldRect, newRect));
}

QList<DashboardBaseItem*> DashboardScene::selectedItems() const
{
    QList<DashboardBaseItem*> result;
    const auto all = QGraphicsScene::selectedItems();
    result.reserve(all.size());
    for (QGraphicsItem* item : all) {
        if (auto* dbi = dynamic_cast<DashboardBaseItem*>(item))
            result.append(dbi);
    }
    return result;
}
