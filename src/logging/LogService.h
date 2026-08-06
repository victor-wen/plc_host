#pragma once

#include <QMutex>
#include <QObject>
#include <QString>
#include <QVariant>

// 文件日志服务（MON-06）。
// 三个日志文件：operation.log（操作）、communication.log（通信）、app.log（应用）。
// 行格式：[2026-08-06 14:30:00.123] [LEVEL] message
// 轮转：单文件达到 maxFileSizeBytes（默认 5MB）时按 .1.log → .2.log 两代轮转；
// 过期清理：按 retentionDays（默认 30 天）删除过期 *.log（含轮转文件）。
// 线程安全：内部 QMutex 串行化全部文件操作。
class LogService : public QObject {
    Q_OBJECT
public:
    enum class Level { Debug, Info, Warning, Error, Critical };
    Q_ENUM(Level)

    explicit LogService(const QString& logDir, QObject* parent = nullptr);

    void setMaxFileSizeBytes(qint64 bytes);   // 轮转阈值，默认 5MB
    void setRetentionDays(int days);          // 过期天数，默认 30

    void logOperation(int tagId, const QVariant& oldVal, const QVariant& newVal,
                      bool success, const QString& error);
    void logComm(Level level, const QString& message);
    void logApp(Level level, const QString& message);

    // 主动触发过期日志清理（写入时亦会按约 1 小时间隔自动清理）
    void cleanupOldLogs();

private:
    void writeLine(const QString& fileName, Level level, const QString& message);
    void rotateIfNeeded(const QString& fileName);
    void maybeCleanupOldLogs();
    void cleanupOldLogsUnlocked();

    QString m_logDir;
    qint64 m_maxBytes = 5LL * 1024 * 1024;
    int m_retentionDays = 30;
    qint64 m_lastCleanupMs = 0;
    mutable QMutex m_mutex;
};
