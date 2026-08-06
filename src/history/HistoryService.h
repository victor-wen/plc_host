#pragma once

#include <QObject>
#include <QMutex>
#include <QString>
#include <QDateTime>
#include <QVector>

#include "domain/TagValue.h"

// 历史数据服务：由采集引擎的数据库线程持有。
// 通过 dbConnectionName 使用调用线程预先建立好的 SQLite 连接
// （SQLite 每线程独立连接，WAL 模式）。
// 提供批量事务写入、时间段查询和按保留天数分批清理。
class HistoryService : public QObject {
    Q_OBJECT
public:
    explicit HistoryService(const QString& dbConnectionName, QObject* parent = nullptr);

    // 放入内存缓冲；缓冲达到 kBatchSize 时自动 flush
    void enqueueSample(const TagValue& tv);

    // 强制将缓冲区写入数据库（单事务批量 INSERT）
    void flush();

    // 按 tag 和时间段查询，结果按 sampled_at 升序
    QVector<TagValue> query(int tagId, const QDateTime& from, const QDateTime& to);

    // 分批删除 retentionDays 天前的数据（每批 1000 行，批间间隔 100ms）
    void cleanOldData(int retentionDays = 90);

private:
    void writeBatch(const QVector<TagValue>& batch);

    QString m_connectionName;
    QVector<TagValue> m_buffer;
    QMutex m_mutex;
    static constexpr int kBatchSize = 500;
};
