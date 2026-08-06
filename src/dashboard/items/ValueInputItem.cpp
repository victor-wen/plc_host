#include "dashboard/items/ValueInputItem.h"

#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ValueInputItem::ValueInputItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("valueInput");
    // 运行模式双击输入依赖鼠标事件，编辑模式的选择/移动由基类处理。
    setAcceptedMouseButtons(Qt::LeftButton);
}

QRectF ValueInputItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void ValueInputItem::setTagValue(const QVariant& value, Quality quality)
{
    m_value = value;
    m_quality = quality;
    update();
}

bool ValueInputItem::hasValue() const
{
    return m_value.isValid();
}

QString ValueInputItem::displayText() const
{
    if (!m_value.isValid())
        return QStringLiteral("--");

    bool ok = false;
    const double number = m_value.toDouble(&ok);
    if (!ok)
        return QStringLiteral("--");
    return QString::number(number, 'f', m_precision);
}

void ValueInputItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_editMode) {
        // 编辑模式：交给基类（选中/拖动），不弹输入框。
        QGraphicsObject::mouseDoubleClickEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton
        && boundingRect().contains(event->pos()))
        promptForValue();
    event->accept();
}

void ValueInputItem::promptForValue()
{
    // 初始值为最近注入的运行值；未取值时从 min 开始。
    double current = m_min;
    if (m_value.isValid()) {
        bool ok = false;
        const double value = m_value.toDouble(&ok);
        if (ok)
            current = value;
    }

    bool ok = false;
    const double result = QInputDialog::getDouble(
        nullptr, tr("输入数值"), tr("请输入新值 (%1 ~ %2):").arg(m_min).arg(m_max),
        current, m_min, m_max, m_precision, &ok);

    // 对话框已按 [min,max] 限制输入，这里再校验一次保证范围不变量。
    if (ok && result >= m_min && result <= m_max) {
        m_value = result;
        m_quality = Quality::Good;
        update();
        emit valueSubmitted(m_tagId, result);
    }
}

void ValueInputItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                           QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF rect = boundingRect();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 占位输入框样式：深色圆角框 + 虚线描边 + 居中数值文本。
    painter->setBrush(QColor(0x2A, 0x2E, 0x38));
    painter->setPen(QPen(QColor(0x5A, 0x64, 0x75), 1, Qt::DashLine));
    painter->drawRoundedRect(rect, 4, 4);

    QFont font;
    font.setPixelSize(14);
    painter->setFont(font);
    painter->setPen(QColor(0xE0, 0xE3, 0xEA));
    painter->drawText(rect, Qt::AlignCenter, displayText());
}

QJsonObject ValueInputItem::serialize() const
{
    QJsonObject cfg = config;
    cfg.insert(QStringLiteral("tagId"), m_tagId);
    cfg.insert(QStringLiteral("min"), m_min);
    cfg.insert(QStringLiteral("max"), m_max);
    cfg.insert(QStringLiteral("precision"), m_precision);
    return cfg;
}

void ValueInputItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    m_tagId = cfg.value(QStringLiteral("tagId")).toInt(-1);
    m_min = cfg.value(QStringLiteral("min")).toDouble(0.0);
    m_max = cfg.value(QStringLiteral("max")).toDouble(100.0);
    m_precision = cfg.value(QStringLiteral("precision")).toInt(1);
}
