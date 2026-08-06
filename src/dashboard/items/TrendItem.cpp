#include "dashboard/items/TrendItem.h"

#include <QFont>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

TrendItem::TrendItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("trend");
}

QRectF TrendItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

int TrendItem::historySeconds() const
{
    return m_historySeconds;
}

void TrendItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF rect = boundingRect();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 背景 + 边框，构成趋势区域容器。
    painter->setBrush(QColor(0x1E, 0x21, 0x28));
    painter->setPen(QPen(QColor(0x5A, 0x64, 0x75), 1));
    painter->drawRect(rect);

    // 绘图区：留边距（左/下轴在绘图区边界）。
    const QRectF plot = rect.adjusted(8, 8, -8, -8);
    painter->setClipRect(plot);

    // 水平网格线（3 条虚线，视觉占位）。
    QPen gridPen(QColor(0x2A, 0x2E, 0x38), 1, Qt::DashLine);
    painter->setPen(gridPen);
    const qreal rows = 3;
    for (int i = 1; i <= rows; ++i) {
        const qreal y = plot.top() + plot.height() * i / (rows + 1);
        painter->drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    // 坐标轴：左轴 + 底轴（实线，稍亮）。
    painter->setClipRect(QRectF());
    painter->setPen(QPen(QColor(0x8E, 0x93, 0x9E), 1));
    painter->drawLine(plot.bottomLeft(), plot.topLeft());
    painter->drawLine(plot.bottomLeft(), plot.bottomRight());

    // 提示文字：DASH-06 仅视觉占位，实时曲线在 MON-03 趋势服务完成后实现。
    QFont font;
    font.setPixelSize(12);
    painter->setFont(font);
    painter->setPen(QColor(0x8E, 0x93, 0x9E));
    painter->drawText(rect, Qt::AlignCenter, QStringLiteral("Trend"));
}

QJsonObject TrendItem::serialize() const
{
    QJsonObject cfg = config;
    cfg.insert(QStringLiteral("tagId"), m_tagId);
    cfg.insert(QStringLiteral("historySeconds"), m_historySeconds);
    return cfg;
}

void TrendItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    m_tagId = cfg.value(QStringLiteral("tagId")).toInt(-1);
    m_historySeconds = cfg.value(QStringLiteral("historySeconds")).toInt(60);
}
