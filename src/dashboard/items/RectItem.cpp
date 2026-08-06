#include "dashboard/items/RectItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

RectItem::RectItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("rect");
}

QRectF RectItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void RectItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                     QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(m_fillColor);
    painter->setPen(QPen(m_borderColor, m_borderWidth));
    painter->drawRect(boundingRect());
}

QJsonObject RectItem::serialize() const
{
    // 无业务 config 属性；表现层属性在 commonStyle（外部直接持久化）。
    return config;
}

void RectItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    // 表现层属性来自 commonStyle（场景工厂已赋值）。
    m_fillColor = QColor(commonStyle.value(QStringLiteral("fillColor"))
                             .toString(QStringLiteral("#C5CFD8")));
    m_borderColor = QColor(commonStyle.value(QStringLiteral("borderColor"))
                               .toString(QStringLiteral("#526170")));
    m_borderWidth = commonStyle.value(QStringLiteral("borderWidth")).toInt(1);
}
