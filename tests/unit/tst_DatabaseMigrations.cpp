#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include "storage/DatabaseMigrator.h"

class DatabaseMigrationsTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tempDir;

private slots:
    void migrate_creates_all_tables()
    {
        QString path = m_tempDir.path() + "/test.db";
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_migrate1");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            {
                QSqlQuery q(db);
                q.exec("PRAGMA journal_mode=WAL");
            }

            QVERIFY(DatabaseMigrator::migrate(db));

            QStringList tables = {"plc_config", "tags", "dashboard_pages", "dashboard_items",
                                  "alarm_rules", "alarm_events", "history_samples",
                                  "recipes", "recipe_items", "operation_logs", "app_settings",
                                  "schema_migrations"};
            for (const auto& t : tables) {
                QSqlQuery q(db);
                q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='" + t + "'");
                QVERIFY2(q.next(), qPrintable("Table missing: " + t));
            }
            db.close();
        }
        QSqlDatabase::removeDatabase("test_migrate1");
    }

    void migrate_idempotent()
    {
        QString path = m_tempDir.path() + "/test2.db";
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_migrate2");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            {
                QSqlQuery q(db);
                q.exec("PRAGMA journal_mode=WAL");
            }

            QVERIFY(DatabaseMigrator::migrate(db));
            QVERIFY(DatabaseMigrator::migrate(db));
            db.close();
        }
        QSqlDatabase::removeDatabase("test_migrate2");
    }

    void currentVersion_returns_1_after_migration()
    {
        QString path = m_tempDir.path() + "/test3.db";
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_migrate3");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            {
                QSqlQuery q(db);
                q.exec("PRAGMA journal_mode=WAL");
            }

            QVERIFY(DatabaseMigrator::migrate(db));
            QCOMPARE(DatabaseMigrator::currentVersion(db), 1);
            db.close();
        }
        QSqlDatabase::removeDatabase("test_migrate3");
    }
};

QTEST_MAIN(DatabaseMigrationsTest)
#include "tst_DatabaseMigrations.moc"
