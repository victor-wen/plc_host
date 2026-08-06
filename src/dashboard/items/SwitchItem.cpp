#include "dashboard/items/SwitchItem.h"

#include <QColor>
#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

SwitchItem::SwitchItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("switch");
    // 运行模式下仍接收左键点击以触发切换（编辑模式的移动/选择由基类处理）。
    setAcceptedMouseButtons(Qt::LeftButton);
}

QRectF SwitchItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void SwitchItem::setOn(bool on)
{
    if (m_on == on)
        return;
    m_on = on;
    update();
}

bool SwitchItem::isOn() const
{
    return m_on;
}

void SwitchItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_editMode) {
        // 编辑模式：交给基类（选中/拖动），不切换。
        QGraphicsObject::mousePressEvent(event);
        return;
    }
    event->accept();
}

void SwitchItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_editMode) {
        QGraphicsObject::mouseReleaseEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton
        && boundingRect().contains(event->pos())) {
        m_on = !m_on;
        update();
        emit toggled(m_tagId, m_on);
        event->accept();
        return;
    }
    event->ignore();
}

void SwitchItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                       QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF rect = boundingRect();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // ON 亮绿、OFF 深灰；边框描边保证深色画布下可见。
    const QColor bg = m_on ? QColor(QStringLiteral("#34C759"))
                           : QColor(QStringLiteral("#3A4854"));
    painter->setBrush(bg);
    painter->setPen(QPen(QColor(QStringLiteral("#17212B")), 1));
    painter->drawRoundedRect(rect, 4, 4);

    QFont font;
    font.setPixelSize(12);
    painter->setFont(font);
    painter->setPen(QColor(QStringLiteral("#FFFFFF")));
    painter->drawText(rect, Qt::AlignCenter, m_on ? QStringLiteral("ON")
                                                  : QStringLiteral("OFF"));
}

QJsonObject SwitchItem::serialize() const
{
    QJsonObject cfg = config;
    cfg.insert(QStringLiteral("tagId"), m_tagId);
    cfg.insert(QStringLiteral("onValue"), QJsonValue::fromVariant(m_onValue));
    cfg.insert(QStringLiteral("offValue"), QJsonValue::fromVariant(m_offValue));
    return cfg;
}

void SwitchItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    m_tagId = cfg.value(QStringLiteral("tagId")).toInt(-1);
    m_onValue = cfg.value(QStringLiteral("onValue")).toVariant();
    m_offValue = cfg.value(QStringLiteral("offValue")).toVariant();
}
