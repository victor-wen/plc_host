#include "dashboard/items/TextItem.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

TextItem::TextItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("text");
}

QRectF TextItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void TextItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                     QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // 单行文字：字体大小来自 commonStyle["fontSize"]，颜色来自 commonStyle["color"]。
    const qreal fontSize = commonStyle.value(QStringLiteral("fontSize")).toDouble(12.0);
    QFont font;
    font.setPixelSize(qMax(1, qRound(fontSize)));
    painter->setFont(font);

    const QString colorName =
        commonStyle.value(QStringLiteral("color")).toString(QStringLiteral("#17212B"));
    painter->setPen(QColor(colorName));
    painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
}

QString TextItem::text() const
{
    return m_text;
}

void TextItem::setText(const QString& text)
{
    if (m_text == text)
        return;
    m_text = text;
    config.insert(QStringLiteral("text"), text);
    update();
}

QJsonObject TextItem::serialize() const
{
    QJsonObject cfg = config;
    cfg.insert(QStringLiteral("text"), m_text);
    return cfg;
}

void TextItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    m_text = cfg.value(QStringLiteral("text")).toString(QStringLiteral("Text"));
}
