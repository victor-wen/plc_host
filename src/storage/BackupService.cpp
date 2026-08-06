#include "storage/BackupService.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "storage/DatabaseMigrator.h"

namespace {
// 与本项目 schema 兼容的最低迁移版本。
constexpr int kMinCompatibleVersion = 1;

// 版本校验要求存在的核心表。
const QStringList kRequiredTables = {
    QStringLiteral("tags"),         QStringLiteral("recipes"),
    QStringLiteral("recipe_items"), QStringLiteral("operation_logs"),
    QStringLiteral("app_settings"),
};

bool openTempConnection(const QString& connName, const QString& dbPath, QSqlDatabase& out)
{
    out = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    out.setDatabaseName(dbPath);
    return out.open();
}
} // namespace

BackupService::BackupService(const QString& dbPath, const QString& connectionName)
    : m_dbPath(dbPath)
    , m_connectionName(connectionName)
{
}

bool BackupService::backup(const QString& filePath)
{
    if (m_dbPath.isEmpty() || filePath.isEmpty())
        return false;
    if (!QFile::exists(m_dbPath))
        return false;

    // VACUUM INTO 要求目标不存在；得到的一致性快照已含 WAL 中的已提交数据
    QFile::remove(filePath);

    const QString connName = QStringLiteral("backup_svc_%1").arg(++m_connCounter);
    bool ok = false;
    {
        QSqlDatabase db;
        if (openTempConnection(connName, m_dbPath, db)) {
            QString escaped = filePath;
            escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral("VACUUM INTO '%1'").arg(escaped));
            if (!ok)
                qWarning() << "backup VACUUM INTO failed:" << q.lastError().text();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return ok && QFile::exists(filePath);
}

bool BackupService::restore(const QString& filePath)
{
    if (filePath.isEmpty() || !QFile::exists(filePath))
        return false;

    // 1) 恢复前先备份当前 DB
    if (QFile::exists(m_dbPath)) {
        const QString preBackupPath = m_dbPath + QStringLiteral(".pre-restore.") +
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmsszzz")) +
            QStringLiteral(".db");
        if (!backup(preBackupPath))
            return false;
    }

    // 2) 验证目标版本兼容
    if (!verifySchema(filePath))
        return false;

    // 3) 替换：关闭既有连接 → 删除旧文件（含 WAL/SHM）→ 复制 → 重开连接
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, /*open=*/false);
    if (!db.isValid()) {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(m_dbPath);
    }
    if (db.isOpen())
        db.close();

    QFile::remove(m_dbPath);
    QFile::remove(m_dbPath + QStringLiteral("-wal"));
    QFile::remove(m_dbPath + QStringLiteral("-shm"));
    if (!QFile::copy(filePath, m_dbPath))
        return false;

    return db.open();
}

bool BackupService::verifySchema(const QString& filePath)
{
    if (filePath.isEmpty() || !QFile::exists(filePath))
        return false;

    const QString connName = QStringLiteral("verify_svc_%1").arg(++m_connCounter);
    bool ok = false;
    {
        QSqlDatabase db;
        if (openTempConnection(connName, filePath, db)) {
            QSqlQuery q(db);
            if (q.exec(QStringLiteral("SELECT MAX(version) FROM schema_migrations")) && q.next())
                ok = q.value(0).toInt() >= kMinCompatibleVersion;

            if (ok) {
                for (const QString& table : kRequiredTables) {
                    QSqlQuery t(db);
                    t.prepare(QStringLiteral(
                        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?"));
                    t.addBindValue(table);
                    if (!t.exec() || !t.next()) {
                        ok = false;
                        break;
                    }
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return ok;
}
