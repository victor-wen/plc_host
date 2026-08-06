#include "dashboard/items/LedItem.h"

#include <QFont>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

LedItem::LedItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("led");
}

QRectF LedItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void LedItem::setOn(bool on)
{
    if (m_on == on)
        return;
    m_on = on;
    update();
}

bool LedItem::isOn() const
{
    return m_on;
}

QString LedItem::label() const
{
    return m_label;
}

void LedItem::setLabel(const QString& label)
{
    if (m_label == label)
        return;
    m_label = label;
    config.insert(QStringLiteral("label"), label);
    update();
}

void LedItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                    QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF rect = boundingRect();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 圆形灯：直径 = min(width, height)，水平居中。
    const qreal diameter = qMin(rect.width(), rect.height());
    const QRectF circle(rect.center().x() - diameter / 2.0,
                        rect.center().y() - diameter / 2.0,
                        diameter, diameter);
    painter->setBrush(m_on ? m_onColor : m_offColor);
    painter->setPen(QPen(QColor(0x5A, 0x64, 0x75), 1));
    painter->drawEllipse(circle);

    // 标签文字显示在组件底部（若配置了 label）。
    if (!m_label.isEmpty()) {
        QFont font;
        font.setPixelSize(10);
        painter->setFont(font);
        painter->setPen(QColor(QStringLiteral("#17212B")));
        painter->drawText(rect.adjusted(0, 0, 0, -2),
                          Qt::AlignHCenter | Qt::AlignBottom, m_label);
    }
}

QJsonObject LedItem::serialize() const
{
    QJsonObject cfg = config;
    cfg.insert(QStringLiteral("tagId"), m_tagId);
    cfg.insert(QStringLiteral("onColor"), m_onColor.name());
    cfg.insert(QStringLiteral("offColor"), m_offColor.name());
    cfg.insert(QStringLiteral("label"), m_label);
    return cfg;
}

void LedItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    m_tagId = cfg.value(QStringLiteral("tagId")).toInt(-1);
    m_onColor = QColor(cfg.value(QStringLiteral("onColor"))
                           .toString(QStringLiteral("#34C759")));
    m_offColor = QColor(cfg.value(QStringLiteral("offColor"))
                            .toString(QStringLiteral("#8E8E93")));
    m_label = cfg.value(QStringLiteral("label")).toString();
}
