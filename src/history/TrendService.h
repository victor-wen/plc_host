#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>

#include "domain/TagValue.h"

QT_BEGIN_NAMESPACE
class QChart;
class QDateTimeAxis;
class QLineSeries;
class QValueAxis;
QT_END_NAMESPACE

// 趋势服务 (MON-03)：UI 线程 QObject。
// 每个订阅的 tag 对应一条 QLineSeries，onTagValueUpdated() 追加实时数据点，
// 并按历史窗口 (historySeconds) 裁剪过期点与限制点数（500ms 采样 ≈ 2 点/秒）。
// 所有 series 汇总到一个 QChart，供 TrendWidget 直接展示。
class TrendService : public QObject {
    Q_OBJECT
public:
    explicit TrendService(QObject* parent = nullptr);
    ~TrendService() override;

    // 订阅 tag：不存在则新建 QLineSeries 并加入 chart；
    // 已订阅则仅更新历史窗口（保留已有点，裁剪到新窗口）。
    void subscribe(int tagId, int historySeconds = 300);

    // 取消订阅：从 chart 移除并删除对应 series。
    void unsubscribe(int tagId);

    bool isSubscribed(int tagId) const;

    // 更新订阅的历史窗口（秒），立即裁剪到新窗口。
    void setHistoryWindow(int tagId, int historySeconds);

    // 返回包含全部已订阅 series 的图表（本服务所有）。
    QChart* chart() const;

    // tagId 对应的 series；未订阅返回 nullptr。
    QLineSeries* seriesFor(int tagId) const;

public slots:
    // 接收实时采样（可来自通信线程，经 QueuedConnection 转回 UI 线程）。
    // 仅处理已订阅的 tag，并发出 pointAdded。
    void onTagValueUpdated(const TagValue& tv);

signals:
    // 已订阅 tag 追加新点时发出。timestampMs 为 Unix 毫秒时间戳。
    void pointAdded(int tagId, qint64 timestampMs, double value);

    // 订阅状态变化（true=订阅，false=取消）。
    void subscriptionChanged(int tagId, bool subscribed);

private:
    struct SeriesEntry {
        QLineSeries* series = nullptr;
        int historySeconds = 300;
        int maxPoints = 600;   // historySeconds * 2
    };

    void appendPoint(int tagId, qint64 timestampMs, double value);
    void pruneAndUpdateAxes();
    void ensureAxes();

    QHash<int, SeriesEntry> m_entries;
    QChart* m_chart = nullptr;
    QDateTimeAxis* m_axisX = nullptr;
    QValueAxis* m_axisY = nullptr;
};
