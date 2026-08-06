#pragma once

#include <QColor>
#include <QCursor>
#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QJsonObject>
#include <QList>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QtGlobal>

class DashboardBaseItem;

// 8 方向缩放手柄（DASH-03）：组件四角/四边的 8x8 小方块子项。拖拽手柄时按
// 方向调整宿主组件的 setSize 与 setPos，并强制最小尺寸 kMinWidth x kMinHeight。
// 手柄为普通 QGraphicsItem（无 QObject/信号需求），运行模式由宿主隐藏。
class ResizeHandle : public QGraphicsItem {
public:
    enum Direction {
        TopLeft, Top, TopRight,
        Left, Right,
        BottomLeft, Bottom, BottomRight
    };

    explicit ResizeHandle(Direction direction, DashboardBaseItem* owner);

    Direction direction() const { return m_direction; }

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    static constexpr qreal kHandleSize = 8.0; // 手柄边长

    Direction m_direction;
    DashboardBaseItem* m_owner;
    QPointF m_startScenePos; // 拖拽起始的鼠标场景坐标
    QRectF m_startRect;      // 拖拽起始的宿主场景边界
    bool m_dragging = false;
};

// 所有看板组件的 QGraphicsObject 基类 (docs/architecture/interfaces.md §9, Phase 2 DASH-02)
// 持有组件的元数据（itemId/itemType/commonStyle/config/schemaVersion）以及编辑/运行
// 模式状态：编辑模式可移动/可选中，运行模式锁定。位置由 setPos() 表示左上角，
// 尺寸由 boundingRect() 提供（DASH-03 缩放手柄会修改 m_width/m_height）。
// 编辑模式持有 8 个 ResizeHandle 子项；itemChange 实现多选拖动同步（DASH-03）。
class DashboardBaseItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit DashboardBaseItem(QGraphicsItem* parent = nullptr);

    // DASH-03: 组件最小尺寸。setSize 与缩放手柄共同保证该下限。
    static constexpr qreal kMinWidth = 40;
    static constexpr qreal kMinHeight = 30;

    int itemId = -1;
    QString itemType;
    QJsonObject commonStyle;
    QJsonObject config;
    int schemaVersion = 1;

    // 切换编辑/运行模式。编辑模式下启用 ItemIsMovable/ItemIsSelectable 并显示
    // 缩放手柄；运行模式下两者强制关闭且隐藏手柄。
    virtual void setEditMode(bool editing);
    bool isEditMode() const;

    QRectF boundingRect() const override;

    // 修改组件尺寸（内部先 prepareGeometryChange）。宽度/高度被钳制到最小尺寸，
    // 缩放手柄位置同步刷新。供场景工厂与 DASH-03 缩放手柄使用。
    void setSize(qreal width, qreal height);

    // 全部 8 个缩放手柄（子项，随组件移动）。
    QList<ResizeHandle*> resizeHandles() const { return m_handles; }

    // 序列化：将组件实时属性写入 config 副本并返回（const 安全；调用方保存或
    // 撤销快照时使用，见 RemoveItemCommand/DASH-10 保存流程）。DASH-05 起
    // 各具体组件覆盖：业务属性（text/tagId/precision...）写入 config；
    // 表现层属性已在 commonStyle（由 dashboardItemToJson 单独持久化）。
    virtual QJsonObject serialize() const;

    // 反序列化：从 config 恢复组件业务属性，并读取 commonStyle 恢复表现层
    // 属性。场景工厂创建组件后调用（addItem 尾部）。损坏的 config 由上层
    // 降级为 errorPlaceholder（DASH-10），本方法不抛错。
    virtual void deserialize(const QJsonObject& config);

protected:
    bool m_editMode = true;

    // 组件尺寸（项坐标，左上角 = (0,0)，位置由 setPos 决定）。
    qreal m_width = 100;
    qreal m_height = 100;

    // 运行模式防御 + 多选拖动同步（DASH-03）。
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    void updateHandlePositions();
    QList<ResizeHandle*> m_handles;
};

// ---- 缩放手柄实现 ----

inline ResizeHandle::ResizeHandle(Direction direction, DashboardBaseItem* owner)
    : QGraphicsItem(owner)
    , m_direction(direction)
    , m_owner(owner)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    switch (direction) {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Top:
    case Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case Left:
    case Right:
        setCursor(Qt::SizeHorCursor);
        break;
    }
}

inline QRectF ResizeHandle::boundingRect() const
{
    // 以 setPos 位置为中心（手柄跨越组件边缘）。
    return QRectF(-kHandleSize / 2.0, -kHandleSize / 2.0, kHandleSize, kHandleSize);
}

inline void ResizeHandle::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                                QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(QColor(0xFF, 0xFF, 0xFF));
    painter->setPen(QPen(QColor(0x40, 0x45, 0x55), 1));
    painter->drawRect(boundingRect());
}

// ---- 组件基类实现 ----

inline DashboardBaseItem::DashboardBaseItem(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    // 默认处于编辑模式：可移动、可选中；几何变化通知用于网格吸附等（DASH-03）。
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);

    // DASH-03: 8 个缩放手柄子项（四角 + 四边），随组件移动/旋转。
    m_handles = {
        new ResizeHandle(ResizeHandle::TopLeft, this),
        new ResizeHandle(ResizeHandle::Top, this),
        new ResizeHandle(ResizeHandle::TopRight, this),
        new ResizeHandle(ResizeHandle::Left, this),
        new ResizeHandle(ResizeHandle::Right, this),
        new ResizeHandle(ResizeHandle::BottomLeft, this),
        new ResizeHandle(ResizeHandle::Bottom, this),
        new ResizeHandle(ResizeHandle::BottomRight, this),
    };
    updateHandlePositions();
}

inline void DashboardBaseItem::setEditMode(bool editing)
{
    if (m_editMode == editing)
        return;
    m_editMode = editing;
    setFlag(ItemIsMovable, editing);
    setFlag(ItemIsSelectable, editing);
    for (auto* handle : m_handles)
        handle->setVisible(editing);
}

inline bool DashboardBaseItem::isEditMode() const
{
    return m_editMode;
}

inline QRectF DashboardBaseItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

inline void DashboardBaseItem::setSize(qreal width, qreal height)
{
    prepareGeometryChange();
    m_width = qMax(kMinWidth, width);
    m_height = qMax(kMinHeight, height);
    updateHandlePositions();
}

inline QJsonObject DashboardBaseItem::serialize() const
{
    return config;
}

inline void DashboardBaseItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
}

inline void DashboardBaseItem::updateHandlePositions()
{
    const qreal w = m_width;
    const qreal h = m_height;
    for (auto* handle : m_handles) {
        QPointF center;
        switch (handle->direction()) {
        case ResizeHandle::TopLeft:
            center = QPointF(0, 0);
            break;
        case ResizeHandle::Top:
            center = QPointF(w / 2.0, 0);
            break;
        case ResizeHandle::TopRight:
            center = QPointF(w, 0);
            break;
        case ResizeHandle::Left:
            center = QPointF(0, h / 2.0);
            break;
        case ResizeHandle::Right:
            center = QPointF(w, h / 2.0);
            break;
        case ResizeHandle::BottomLeft:
            center = QPointF(0, h);
            break;
        case ResizeHandle::Bottom:
            center = QPointF(w / 2.0, h);
            break;
        case ResizeHandle::BottomRight:
            center = QPointF(w, h);
            break;
        }
        handle->setPos(center);
    }
}

inline QVariant DashboardBaseItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemFlagsChange && !m_editMode) {
        // 运行模式下强制禁止交互，防止外部误调 setFlags 破坏锁定。
        const Qt::ItemFlags flags = value.value<Qt::ItemFlags>();
        return QVariant::fromValue(flags & ~(ItemIsMovable | ItemIsSelectable));
    }

    if (change == ItemPositionChange && m_editMode) {
        // DASH-03 多选拖动同步：仅当本项是当前鼠标抓取者（用户正在拖拽它）时，
        // 其余选中项按相同 delta 跟随移动。程序化 setPos（对齐/粘贴/撤销栈命令）
        // 时本项不是鼠标抓取者，不触发同步，避免递归与二次位移。
        QGraphicsScene* s = scene();
        if (s && s->mouseGrabberItem() == this) {
            const QPointF newPos = value.toPointF();
            const QPointF delta = newPos - pos();
            if (!delta.isNull()) {
                const QList<QGraphicsItem*> selected = s->selectedItems();
                for (QGraphicsItem* other : selected) {
                    if (other == this)
                        continue;
                    if (auto* dbi = dynamic_cast<DashboardBaseItem*>(other))
                        dbi->setPos(dbi->pos() + delta);
                }
            }
        }
    }

    return QGraphicsObject::itemChange(change, value);
}

// ---- 缩放手柄交互（依赖宿主完整类型，故置于组件定义之后） ----

inline void ResizeHandle::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_startScenePos = event->scenePos();
    m_startRect = QRectF(m_owner->scenePos(), m_owner->boundingRect().size());
    m_dragging = true;
    m_owner->setSelected(true);
    event->accept();
}

inline void ResizeHandle::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_dragging)
        return;

    const QPointF delta = event->scenePos() - m_startScenePos;
    QRectF rect = m_startRect;

    const bool onLeft = m_direction == TopLeft || m_direction == Left || m_direction == BottomLeft;
    const bool onRight = m_direction == TopRight || m_direction == Right || m_direction == BottomRight;
    const bool onTop = m_direction == TopLeft || m_direction == Top || m_direction == TopRight;
    const bool onBottom = m_direction == BottomLeft || m_direction == Bottom || m_direction == BottomRight;

    if (onLeft)
        rect.setLeft(rect.left() + delta.x());
    if (onRight)
        rect.setRight(rect.right() + delta.x());
    if (onTop)
        rect.setTop(rect.top() + delta.y());
    if (onBottom)
        rect.setBottom(rect.bottom() + delta.y());

    // 强制最小尺寸：钳制被拖动的边缘（对角拖拽时拖拽角保持自由，对边固定）。
    if (rect.width() < DashboardBaseItem::kMinWidth) {
        if (onLeft)
            rect.setLeft(rect.right() - DashboardBaseItem::kMinWidth);
        else
            rect.setRight(rect.left() + DashboardBaseItem::kMinWidth);
    }
    if (rect.height() < DashboardBaseItem::kMinHeight) {
        if (onTop)
            rect.setTop(rect.bottom() - DashboardBaseItem::kMinHeight);
        else
            rect.setBottom(rect.top() + DashboardBaseItem::kMinHeight);
    }

    m_owner->setPos(rect.topLeft());
    m_owner->setSize(rect.width(), rect.height());
    event->accept();
}

inline void ResizeHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    m_dragging = false;
    event->accept();
}
