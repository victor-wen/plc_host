#include "logging/LogService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>

namespace {

// 自动清理的最小间隔：1 小时（避免每次写入都扫描目录）。
constexpr qint64 kCleanupIntervalMs = 3600 * 1000;

QString levelName(LogService::Level level)
{
    switch (level) {
    case LogService::Level::Debug:
        return QStringLiteral("DEBUG");
    case LogService::Level::Info:
        return QStringLiteral("INFO");
    case LogService::Level::Warning:
        return QStringLiteral("WARNING");
    case LogService::Level::Error:
        return QStringLiteral("ERROR");
    case LogService::Level::Critical:
        return QStringLiteral("CRITICAL");
    }
    return QStringLiteral("INFO");
}

} // namespace

LogService::LogService(const QString& logDir, QObject* parent)
    : QObject(parent)
    , m_logDir(logDir)
{
    QDir().mkpath(m_logDir);
}

void LogService::setMaxFileSizeBytes(qint64 bytes)
{
    QMutexLocker locker(&m_mutex);
    m_maxBytes = qMax<qint64>(1, bytes);
}

void LogService::setRetentionDays(int days)
{
    QMutexLocker locker(&m_mutex);
    m_retentionDays = qMax(0, days);
}

void LogService::logOperation(int tagId, const QVariant& oldVal, const QVariant& newVal,
                              bool success, const QString& error)
{
    const QString message = QStringLiteral("tagId=%1 old=%2 new=%3 success=%4 error=%5")
                                .arg(tagId)
                                .arg(oldVal.toString())
                                .arg(newVal.toString())
                                .arg(success ? QStringLiteral("true") : QStringLiteral("false"))
                                .arg(error);
    writeLine(QStringLiteral("operation.log"), success ? Level::Info : Level::Error, message);
}

void LogService::logComm(Level level, const QString& message)
{
    writeLine(QStringLiteral("communication.log"), level, message);
}

void LogService::logApp(Level level, const QString& message)
{
    writeLine(QStringLiteral("app.log"), level, message);
}

void LogService::cleanupOldLogs()
{
    QMutexLocker locker(&m_mutex);
    cleanupOldLogsUnlocked();
}

void LogService::writeLine(const QString& fileName, Level level, const QString& message)
{
    QMutexLocker locker(&m_mutex);

    rotateIfNeeded(fileName);
    maybeCleanupOldLogs();

    QFile file(m_logDir + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;
    const QString line = QStringLiteral("[%1] [%2] %3\n")
                             .arg(QDateTime::currentDateTime().toString(
                                      QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                  levelName(level), message);
    file.write(line.toUtf8());
}

void LogService::rotateIfNeeded(const QString& fileName)
{
    const QString path = m_logDir + QLatin1Char('/') + fileName;
    const QFileInfo info(path);
    if (!info.exists() || info.size() < m_maxBytes)
        return;

    QString base = fileName;
    if (base.endsWith(QLatin1String(".log")))
        base.chop(4);

    // 两代轮转：删除最旧 → .1 → .2 → 当前 → .1
    QFile::remove(m_logDir + QLatin1Char('/') + base + QStringLiteral(".2.log"));
    QFile::rename(m_logDir + QLatin1Char('/') + base + QStringLiteral(".1.log"),
                  m_logDir + QLatin1Char('/') + base + QStringLiteral(".2.log"));
    QFile::rename(path, m_logDir + QLatin1Char('/') + base + QStringLiteral(".1.log"));
}

void LogService::maybeCleanupOldLogs()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastCleanupMs < kCleanupIntervalMs)
        return;
    m_lastCleanupMs = now;
    cleanupOldLogsUnlocked();
}

void LogService::cleanupOldLogsUnlocked()
{
    const qint64 cutoff =
        QDateTime::currentDateTimeUtc().addDays(-m_retentionDays).toSecsSinceEpoch();
    QDir dir(m_logDir);
    const QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.log"),
                                                  QDir::Files);
    for (const QFileInfo& fi : files) {
        if (fi.lastModified().toSecsSinceEpoch() < cutoff)
            QFile::remove(fi.absoluteFilePath());
    }
}
