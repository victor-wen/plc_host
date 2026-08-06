#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include "storage/BackupService.h"
#include "storage/DatabaseMigrator.h"

class BackupServiceTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tempDir;
    int m_connCounter = 0;

    struct DbFixture {
        QString connName;
        QString path;
        QSqlDatabase db;
    };

    // 建立独立连接（每测试唯一名称），WAL + 迁移到最新 schema
    DbFixture openDb(const QString& suffix)
    {
        DbFixture f;
        f.connName = QStringLiteral("backup_%1_%2").arg(++m_connCounter).arg(suffix);
        f.path = m_tempDir.path() + "/" + f.connName + ".db";
        f.db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), f.connName);
        f.db.setDatabaseName(f.path);
        f.db.open();
        {
            QSqlQuery q(f.db);
            q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        }
        DatabaseMigrator::migrate(f.db);
        return f;
    }

    void insertTag(QSqlDatabase& db, const QString& name)
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT INTO tags (name, register_type, address, data_type) VALUES (?, 3, 0, 2)"));
        q.addBindValue(name);
        QVERIFY2(q.exec(), qPrintable(q.lastError().text()));
    }

    int countTags(QSqlDatabase& db, const QString& name)
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM tags WHERE name = ?"));
        q.addBindValue(name);
        if (!q.exec() || !q.next())
            return -1;
        return q.value(0).toInt();
    }

private slots:
    void backup_creates_restorable_snapshot()
    {
        DbFixture d = openDb("backup");
        QVERIFY(d.db.isOpen());
        insertTag(d.db, QStringLiteral("alpha"));

        const QString backupPath = m_tempDir.path() + QStringLiteral("/alpha.backup.db");
        BackupService svc(d.path, d.connName);
        QVERIFY(svc.backup(backupPath));
        QVERIFY(QFile::exists(backupPath));
        QVERIFY(svc.verifySchema(backupPath));

        // 备份文件可直接独立打开并含数据
        {
            QSqlDatabase b = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                       QStringLiteral("backup_open"));
            b.setDatabaseName(backupPath);
            QVERIFY(b.open());
            QCOMPARE(DatabaseMigrator::currentVersion(b), 1);
            QCOMPARE(countTags(b, QStringLiteral("alpha")), 1);
            b.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("backup_open"));
    }

    void verifySchema_rejects_invalid_input()
    {
        const QString garbagePath = m_tempDir.path() + QStringLiteral("/garbage.db");
        {
            QFile f(garbagePath);
            f.open(QIODevice::WriteOnly);
            f.write("this is not a sqlite database");
            f.close();
        }

        const QString foreignPath = m_tempDir.path() + QStringLiteral("/foreign.db");
        {
            QSqlDatabase fdb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                        QStringLiteral("foreign_conn"));
            fdb.setDatabaseName(foreignPath);
            fdb.open();
            {
                QSqlQuery q(fdb);
                q.exec(QStringLiteral("CREATE TABLE foo (id INTEGER)"));
            }
            fdb.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("foreign_conn"));

        BackupService svc(QStringLiteral("/nonexistent/main.db"), QStringLiteral("no_conn"));
        QVERIFY(!svc.verifySchema(m_tempDir.path() + QStringLiteral("/missing.db")));
        QVERIFY(!svc.verifySchema(garbagePath));      // 非 SQLite
        QVERIFY(!svc.verifySchema(foreignPath));      // 合法 SQLite 但非本应用 schema
    }

    void restore_replaces_database_and_creates_prerestore_backup()
    {
        // 源库：含 source_tag 的备份
        DbFixture src = openDb("src");
        insertTag(src.db, QStringLiteral("source_tag"));
        const QString backupPath = m_tempDir.path() + QStringLiteral("/src.backup.db");
        BackupService srcSvc(src.path, src.connName);
        QVERIFY(srcSvc.backup(backupPath));

        // 目标库：含 target_tag，将被替换
        DbFixture dst = openDb("dst");
        insertTag(dst.db, QStringLiteral("target_tag"));
        QCOMPARE(countTags(dst.db, QStringLiteral("target_tag")), 1);

        BackupService dstSvc(dst.path, dst.connName);
        QVERIFY(dstSvc.restore(backupPath));
        QVERIFY(dst.db.isOpen());

        // 数据已恢复，旧数据被替换
        QCOMPARE(countTags(dst.db, QStringLiteral("source_tag")), 1);
        QCOMPARE(countTags(dst.db, QStringLiteral("target_tag")), 0);

        // 恢复前自动备份了当前 DB
        const QStringList preBackups =
            QDir(m_tempDir.path()).entryList({QStringLiteral("*.pre-restore.*.db")}, QDir::Files);
        QCOMPARE(preBackups.size(), 1);
    }

    void restore_fails_for_incompatible_target_and_keeps_current()
    {
        DbFixture d = openDb("incompat");
        insertTag(d.db, QStringLiteral("keep_me"));

        const QString garbagePath = m_tempDir.path() + QStringLiteral("/garbage.db");
        {
            QFile f(garbagePath);
            f.open(QIODevice::WriteOnly);
            f.write("garbage");
            f.close();
        }

        BackupService svc(d.path, d.connName);
        QVERIFY(!svc.restore(garbagePath));

        // 当前数据库未受影响，连接仍可用
        QVERIFY(d.db.isOpen());
        QCOMPARE(countTags(d.db, QStringLiteral("keep_me")), 1);
    }
};

QTEST_MAIN(BackupServiceTest)
#include "tst_BackupService.moc"
