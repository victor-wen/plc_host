#include "dashboard/items/ProgressBarItem.h"

#include <QFont>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ProgressBarItem::ProgressBarItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("progress");
}

QRectF ProgressBarItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void ProgressBarItem::setTagValue(const QVariant& value, Quality quality)
{
    m_value = value;
    m_quality = quality;
    update();
}

bool ProgressBarItem::hasValue() const
{
    return m_value.isValid();
}

qreal ProgressBarItem::ratio() const
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

void ProgressBarItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                            QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF rect = boundingRect();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 背景矩形（圆角）。
    painter->setBrush(m_backgroundColor);
    painter->setPen(QPen(QColor(0x5A, 0x64, 0x75), 1));
    painter->drawRoundedRect(rect, 3, 3);

    // 填充矩形：留 2px 内边距，宽度 = ratio * 可用宽度。
    if (m_quality == Quality::Disconnected)
        painter->setBrush(m_barColor.darker(140));
    else
        painter->setBrush(m_barColor);
    painter->setPen(Qt::NoPen);
    const qreal fillWidth = (rect.width() - 4.0) * ratio();
    if (fillWidth > 0.0) {
        painter->drawRoundedRect(QRectF(rect.left() + 2.0, rect.top() + 2.0,
                                        fillWidth, rect.height() - 4.0),
                                 2, 2);
    }

    // 百分比文本显示在条中央。
    if (m_showValue) {
        QFont font;
        font.setPixelSize(12);
        painter->setFont(font);
        painter->setPen(QColor(0xE0, 0xE3, 0xEA));
        const QString text = m_value.isValid()
            ? QStringLiteral("%1%").arg(qRound(ratio() * 100.0))
            : QStringLiteral("--");
        painter->drawText(rect, Qt::AlignCenter, text);
    }
}

QJsonObject ProgressBarItem::serialize() const
{
    QJsonObject cfg = config;
    cfg.insert(QStringLiteral("tagId"), m_tagId);
    cfg.insert(QStringLiteral("min"), m_min);
    cfg.insert(QStringLiteral("max"), m_max);
    return cfg;
}

void ProgressBarItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    m_tagId = cfg.value(QStringLiteral("tagId")).toInt(-1);
    m_min = cfg.value(QStringLiteral("min")).toDouble(0.0);
    m_max = cfg.value(QStringLiteral("max")).toDouble(100.0);
    // 表现层属性来自 commonStyle（场景工厂已赋值）。
    m_barColor = QColor(commonStyle.value(QStringLiteral("barColor"))
                            .toString(QStringLiteral("#34C759")));
    m_backgroundColor = QColor(commonStyle.value(QStringLiteral("backgroundColor"))
                                   .toString(QStringLiteral("#2A2E38")));
    m_showValue = commonStyle.value(QStringLiteral("showValue")).toBool(true);
}
