#include "dashboard/items/ValueItem.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ValueItem::ValueItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("value");
}

QRectF ValueItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void ValueItem::setTagValue(const QVariant& value, Quality quality)
{
    m_value = value;
    m_quality = quality;
    update();
}

bool ValueItem::hasValue() const
{
    return m_value.isValid();
}

QString ValueItem::displayText() const
{
    if (!m_value.isValid())
        return QStringLiteral("--");

    bool ok = false;
    const double number = m_value.toDouble(&ok);
    if (!ok)
        return QStringLiteral("--");

    return m_prefix + QString::number(number, 'f', m_precision) + m_suffix;
}

QColor ValueItem::qualityColor(Quality quality)
{
    switch (quality) {
    case Quality::Good:
        return QColor(QStringLiteral("#34C759"));
    case Quality::Stale:
        return QColor(QStringLiteral("#FF9500"));
    case Quality::Bad:
        return QColor(QStringLiteral("#FF3B30"));
    case Quality::Disconnected:
        break;
    }
    return QColor(QStringLiteral("#8E8E93"));
}

void ValueItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // 数值使用 value_display token（20px）。
    QFont font;
    font.setPixelSize(20);
    painter->setFont(font);
    painter->setPen(qualityColor(m_quality));
    painter->drawText(boundingRect(), Qt::AlignCenter, displayText());
}

QJsonObject ValueItem::serialize() const
{
    QJsonObject cfg = config;
    cfg.insert(QStringLiteral("tagId"), m_tagId);
    cfg.insert(QStringLiteral("precision"), m_precision);
    cfg.insert(QStringLiteral("prefix"), m_prefix);
    cfg.insert(QStringLiteral("suffix"), m_suffix);
    return cfg;
}

void ValueItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    m_tagId = cfg.value(QStringLiteral("tagId")).toInt(-1);
    m_precision = cfg.value(QStringLiteral("precision")).toInt(1);
    m_prefix = cfg.value(QStringLiteral("prefix")).toString();
    m_suffix = cfg.value(QStringLiteral("suffix")).toString();
}
