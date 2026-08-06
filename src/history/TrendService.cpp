#include "history/TrendService.h"

#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtGlobal>
#include <QtMath>

namespace {
constexpr int kPointsPerSecond = 2;   // 默认 500ms 采样
} // namespace

TrendService::TrendService(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<TagValue>("TagValue");

    // QChart 是 QGraphicsWidget，不能以 QObject 为父。无父创建；
    // 展示时由 QChartView::setChart 接管所有权，析构时仅回收未被接管的图表。
    m_chart = new QChart();
    m_chart->setTitle(QStringLiteral("实时趋势"));
    m_chart->legend()->setVisible(true);
    m_chart->setAnimationOptions(QChart::NoAnimation);

    m_axisX = new QDateTimeAxis(this);
    m_axisX->setFormat(QStringLiteral("HH:mm:ss"));
    m_axisX->setTickCount(6);
    m_axisX->setTitleText(QStringLiteral("时间"));
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis(this);
    m_axisY->setTitleText(QStringLiteral("值"));
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
}

TrendService::~TrendService()
{
    // 图表未被 QChartView 接管时在此回收（被接管时归 view 所有，不可重复释放）。
    if (m_chart != nullptr && m_chart->parent() == nullptr)
        delete m_chart;
}

void TrendService::subscribe(int tagId, int historySeconds)
{
    if (m_entries.contains(tagId)) {
        setHistoryWindow(tagId, historySeconds);
        return;
    }

    SeriesEntry entry;
    entry.historySeconds = qMax(1, historySeconds);
    entry.maxPoints = entry.historySeconds * kPointsPerSecond;
    entry.series = new QLineSeries(this);
    entry.series->setName(QStringLiteral("tag_%1").arg(tagId));
    m_chart->addSeries(entry.series);
    entry.series->attachAxis(m_axisX);
    entry.series->attachAxis(m_axisY);

    m_entries.insert(tagId, entry);
    emit subscriptionChanged(tagId, true);
}

void TrendService::unsubscribe(int tagId)
{
    auto it = m_entries.find(tagId);
    if (it == m_entries.end())
        return;
    m_chart->removeSeries(it->series);
    it->series->deleteLater();
    m_entries.erase(it);
    emit subscriptionChanged(tagId, false);
}

bool TrendService::isSubscribed(int tagId) const
{
    return m_entries.contains(tagId);
}

void TrendService::setHistoryWindow(int tagId, int historySeconds)
{
    auto it = m_entries.find(tagId);
    if (it == m_entries.end())
        return;
    it->historySeconds = qMax(1, historySeconds);
    it->maxPoints = it->historySeconds * kPointsPerSecond;
    pruneAndUpdateAxes();
}

QChart* TrendService::chart() const
{
    return m_chart;
}

QLineSeries* TrendService::seriesFor(int tagId) const
{
    auto it = m_entries.constFind(tagId);
    return it == m_entries.constEnd() ? nullptr : it->series;
}

void TrendService::onTagValueUpdated(const TagValue& tv)
{
    if (!m_entries.contains(tv.tagId))
        return;
    bool ok = false;
    const double value = tv.value.toDouble(&ok);
    if (!ok || !qIsFinite(value))
        return;

    const qint64 ms = tv.timestamp.isValid()
        ? tv.timestamp.toMSecsSinceEpoch()
        : QDateTime::currentMSecsSinceEpoch();
    appendPoint(tv.tagId, ms, value);
}

void TrendService::appendPoint(int tagId, qint64 timestampMs, double value)
{
    m_entries[tagId].series->append(static_cast<qreal>(timestampMs), value);
    pruneAndUpdateAxes();
    emit pointAdded(tagId, timestampMs, value);
}

void TrendService::pruneAndUpdateAxes()
{
    if (m_entries.isEmpty())
        return;

    qint64 latest = 0;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        QLineSeries* series = it->series;
        const QVector<QPointF> pts = series->points();
        if (pts.isEmpty())
            continue;

        const qint64 windowMs = qint64(it->historySeconds) * 1000;
        const qint64 newest = qint64(pts.constLast().x());
        const qint64 minX = newest - windowMs;
        latest = qMax(latest, newest);

        int staleCount = 0;
        while (staleCount < pts.size() && qint64(pts.at(staleCount).x()) < minX)
            ++staleCount;
        if (staleCount > 0)
            series->removePoints(0, staleCount);

        const int size = series->count();
        if (size > it->maxPoints)
            series->removePoints(0, size - it->maxPoints);
    }

    // X 轴滑动窗口：以最新时间点为右端。
    if (latest > 0) {
        qint64 windowMs = 0;
        for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it)
            windowMs = qMax(windowMs, qint64(it->historySeconds) * 1000);
        const QDateTime end = QDateTime::fromMSecsSinceEpoch(latest, Qt::UTC);
        m_axisX->setRange(end.addMSecs(-windowMs), end);
    }

    // Y 轴按全部可见点自动缩放（留 5% 边距）。
    double yMin = 0.0;
    double yMax = 0.0;
    bool hasPoint = false;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const QVector<QPointF> pts = it->series->points();
        for (const QPointF& p : pts) {
            if (!hasPoint) {
                yMin = yMax = p.y();
                hasPoint = true;
            } else {
                yMin = qMin(yMin, p.y());
                yMax = qMax(yMax, p.y());
            }
        }
    }
    if (hasPoint) {
        double pad = (yMax - yMin) * 0.05;
        if (qFuzzyIsNull(pad))
            pad = 1.0;
        m_axisY->setRange(yMin - pad, yMax + pad);
    }
}
