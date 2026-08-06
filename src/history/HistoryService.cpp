#include "history/HistoryService.h"

#include <QDebug>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>

namespace {
constexpr int kDeleteBatchSize = 1000;

QString toDbTime(const QDateTime& dt)
{
    return dt.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

QDateTime fromDbTime(const QVariant& raw)
{
    const QString s = raw.toString();
    QDateTime dt = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    if (!dt.isValid())
        dt = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (dt.isValid())
        dt.setTimeSpec(Qt::UTC);
    return dt;
}
} // namespace

HistoryService::HistoryService(const QString& dbConnectionName, QObject* parent)
    : QObject(parent)
    , m_connectionName(dbConnectionName)
{
}

void HistoryService::enqueueSample(const TagValue& tv)
{
    bool needFlush = false;
    {
        QMutexLocker locker(&m_mutex);
        m_buffer.append(tv);
        needFlush = m_buffer.size() >= kBatchSize;
    }
    if (needFlush)
        flush();
}

void HistoryService::flush()
{
    QVector<TagValue> batch;
    {
        QMutexLocker locker(&m_mutex);
        if (m_buffer.isEmpty())
            return;
        batch = m_buffer;
        m_buffer.clear();
    }
    writeBatch(batch);
}

void HistoryService::writeBatch(const QVector<TagValue>& batch)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        qWarning() << "HistoryService: connection not open:" << m_connectionName;
        return;
    }

    if (!db.transaction()) {
        qWarning() << "HistoryService: begin transaction failed:" << db.lastError().text();
        return;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO history_samples (tag_id, value, quality, sampled_at) "
        "VALUES (?, ?, ?, ?)"));
    for (const TagValue& tv : batch) {
        // 位置绑定覆盖旧值（addBindValue 会追加导致参数个数不匹配）
        q.bindValue(0, tv.tagId);
        q.bindValue(1, tv.value.toDouble());
        q.bindValue(2, static_cast<int>(tv.quality));
        q.bindValue(3, toDbTime(tv.timestamp));
        if (!q.exec()) {
            qWarning() << "HistoryService: insert failed:" << q.lastError().text();
            db.rollback();
            return;
        }
    }

    if (!db.commit()) {
        qWarning() << "HistoryService: commit failed:" << db.lastError().text();
        db.rollback();
    }
}

QVector<TagValue> HistoryService::query(int tagId, const QDateTime& from, const QDateTime& to)
{
    QVector<TagValue> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return result;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT tag_id, value, quality, sampled_at FROM history_samples "
        "WHERE tag_id = ? AND sampled_at BETWEEN ? AND ? "
        "ORDER BY sampled_at"));
    q.addBindValue(tagId);
    q.addBindValue(toDbTime(from));
    q.addBindValue(toDbTime(to));
    if (!q.exec()) {
        qWarning() << "HistoryService: query failed:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        TagValue tv;
        tv.tagId = q.value(0).toInt();
        tv.value = q.value(1).toDouble();
        tv.quality = static_cast<Quality>(q.value(2).toInt());
        tv.timestamp = fromDbTime(q.value(3));
        result.append(tv);
    }
    return result;
}

void HistoryService::cleanOldData(int retentionDays)
{
    if (retentionDays <= 0)
        return;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return;

    const QString cutoffModifier = QStringLiteral("-%1 days").arg(retentionDays);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "DELETE FROM history_samples "
        "WHERE id IN (SELECT id FROM history_samples "
        "             WHERE sampled_at < datetime('now', ?) "
        "             ORDER BY id LIMIT %1)")
                  .arg(kDeleteBatchSize));

    // 分批删除：每批最多 kDeleteBatchSize 行，批间间隔 100ms，避免长时间锁库
    int deleted = 0;
    do {
        q.bindValue(0, cutoffModifier);
        if (!q.exec()) {
            qWarning() << "HistoryService: cleanOldData failed:" << q.lastError().text();
            return;
        }
        deleted = q.numRowsAffected();
        if (deleted == kDeleteBatchSize)
            QThread::msleep(100);
    } while (deleted == kDeleteBatchSize);
}
