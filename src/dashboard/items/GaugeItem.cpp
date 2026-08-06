#include "dashboard/items/GaugeItem.h"

#include <QFont>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

GaugeItem::GaugeItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("gauge");
}

QRectF GaugeItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void GaugeItem::setTagValue(const QVariant& value, Quality quality)
{
    m_value = value;
    m_quality = quality;
    update();
}

bool GaugeItem::hasValue() const
{
    return m_value.isValid();
}

qreal GaugeItem::ratio() const
{
    if (!m_value.isValid())
        return 0.0;

    bool ok = false;
    const double value = m_value.toDouble(&ok);
    if (!ok)
        return 0.0;

    const double span = m_max - m_min;
    if (span <= 0.0)
        return 0.0;
    return qBound(0.0, (value - m_min) / span, 1.0);
}

void GaugeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF rect = boundingRect();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 圆弧所在正方形区域（取较短的边，留 2px 内边距），居中。
    const qreal side = qMin(rect.width(), rect.height()) - 4.0;
    const QRectF arcRect(rect.center().x() - side / 2.0,
                         rect.center().y() - side / 2.0,
                         side, side);
    const qreal radius = side / 2.0;
    const QPointF center = arcRect.center();

    // 圆弧线宽随组件尺寸缩放，保证小尺寸下仍可见。
    const qreal penWidth = qBound(4.0, side / 10.0, 16.0);

    // 灰色圆弧背景（QPainter 角度单位 1/16 度）。
    QPen bgPen(QColor(0x2A, 0x2E, 0x38), penWidth, Qt::SolidLine, Qt::FlatCap);
    painter->setPen(bgPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawArc(arcRect, qRound(m_startAngle * 16.0), qRound(m_spanAngle * 16.0));

    // 彩色数值圆弧：扫过 spanAngle * ratio。
    QColor valueColor = m_arcColor;
    if (m_quality == Quality::Disconnected)
        valueColor = valueColor.darker(140);
    QPen valuePen(valueColor, penWidth, Qt::SolidLine, Qt::FlatCap);
    painter->setPen(valuePen);
    const qreal valueSpan = m_spanAngle * ratio();
    if (valueSpan > 0.5)
        painter->drawArc(arcRect, qRound(m_startAngle * 16.0), qRound(valueSpan * 16.0));

    // 指针：从圆心指向当前值角度（与 drawArc 同一映射：点 = 圆心 + r*(cos, sin)）。
    const qreal angleDeg = m_startAngle + m_spanAngle * ratio();
    const qreal rad = qDegreesToRadians(angleDeg);
    const QPointF tip(center.x() + qCos(rad) * (radius - penWidth / 2.0),
                      center.y() + qSin(rad) * (radius - penWidth / 2.0));
    painter->setPen(QPen(QColor(0xE0, 0xE3, 0xEA), qMax(1.5, penWidth / 5.0),
                         Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(center, tip);

    // 中心值文字。
    QFont font;
    font.setPixelSize(qMax(9, qRound(side / 5.0)));
    painter->setFont(font);
    painter->setPen(QColor(0xE0, 0xE3, 0xEA));
    const QString text = m_value.isValid()
        ? QString::number(m_value.toDouble(), 'f', 1)
        : QStringLiteral("0");
    painter->drawText(arcRect.adjusted(0, 0, 0, -side / 4.0),
                      Qt::AlignCenter, text);
}

QJsonObject GaugeItem::serialize() const
{
    QJsonObject cfg = config;
    cfg.insert(QStringLiteral("tagId"), m_tagId);
    cfg.insert(QStringLiteral("min"), m_min);
    cfg.insert(QStringLiteral("max"), m_max);
    cfg.insert(QStringLiteral("startAngle"), m_startAngle);
    cfg.insert(QStringLiteral("spanAngle"), m_spanAngle);
    return cfg;
}

void GaugeItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    m_tagId = cfg.value(QStringLiteral("tagId")).toInt(-1);
    m_min = cfg.value(QStringLiteral("min")).toDouble(0.0);
    m_max = cfg.value(QStringLiteral("max")).toDouble(100.0);
    m_startAngle = cfg.value(QStringLiteral("startAngle")).toDouble(225.0);
    m_spanAngle = cfg.value(QStringLiteral("spanAngle")).toDouble(270.0);
    // 表现层属性来自 commonStyle（场景工厂已赋值）。
    m_arcColor = QColor(commonStyle.value(QStringLiteral("arcColor"))
                            .toString(QStringLiteral("#34C759")));
}
