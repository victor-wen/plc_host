#include "DatabaseMigrator.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool DatabaseMigrator::migrate(QSqlDatabase& db)
{
    // Check version outside transaction
    int version = currentVersion(db);

    if (!db.transaction()) {
        qWarning() << "Failed to start migration transaction:" << db.lastError().text();
        return false;
    }

    // Create migrations table inside transaction
    {
        QSqlQuery q(db);
        if (!q.exec("CREATE TABLE IF NOT EXISTS schema_migrations ("
                    "version INTEGER PRIMARY KEY,"
                    "applied_at TEXT NOT NULL DEFAULT (datetime('now')))")) {
            qWarning() << "Failed to create schema_migrations:" << q.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (version < 1) {
        if (!migrateV1(db)) {
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        qWarning() << "Failed to commit migration:" << db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

int DatabaseMigrator::currentVersion(QSqlDatabase& db)
{
    QSqlQuery q(db);
    q.exec("SELECT MAX(version) FROM schema_migrations");
    if (q.next())
        return q.value(0).toInt();
    return 0;
}

bool DatabaseMigrator::migrateV1(QSqlDatabase& db)
{
    QStringList stmts = {
        "CREATE TABLE IF NOT EXISTS plc_config ("
        "id INTEGER PRIMARY KEY CHECK (id = 1),"
        "name TEXT NOT NULL DEFAULT '',"
        "host TEXT NOT NULL DEFAULT '192.168.1.100',"
        "port INTEGER NOT NULL DEFAULT 502,"
        "unit_id INTEGER NOT NULL DEFAULT 1,"
        "timeout_ms INTEGER NOT NULL DEFAULT 1000,"
        "retries INTEGER NOT NULL DEFAULT 2,"
        "poll_interval_ms INTEGER NOT NULL DEFAULT 500,"
        "auto_connect INTEGER NOT NULL DEFAULT 1)",

        "CREATE TABLE IF NOT EXISTS tags ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "register_type INTEGER NOT NULL,"
        "address INTEGER NOT NULL,"
        "data_type INTEGER NOT NULL,"
        "byte_order INTEGER NOT NULL DEFAULT 0,"
        "word_order INTEGER NOT NULL DEFAULT 0,"
        "bit_position INTEGER NOT NULL DEFAULT 0,"
        "bit_length INTEGER NOT NULL DEFAULT 1,"
        "scale REAL NOT NULL DEFAULT 1.0,"
        "offset REAL NOT NULL DEFAULT 0.0,"
        "unit TEXT NOT NULL DEFAULT '',"
        "read_only INTEGER NOT NULL DEFAULT 0,"
        "poll_group INTEGER NOT NULL DEFAULT 0,"
        "poll_interval_ms INTEGER NOT NULL DEFAULT 500,"
        "history_enabled INTEGER NOT NULL DEFAULT 0,"
        "history_mode INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')))",

        "CREATE TABLE IF NOT EXISTS dashboard_pages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "width INTEGER NOT NULL DEFAULT 1920,"
        "height INTEGER NOT NULL DEFAULT 1080,"
        "background TEXT NOT NULL DEFAULT '',"
        "sort_order INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')))",

        "CREATE TABLE IF NOT EXISTS dashboard_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "page_id INTEGER NOT NULL,"
        "item_type TEXT NOT NULL,"
        "x REAL NOT NULL DEFAULT 0,"
        "y REAL NOT NULL DEFAULT 0,"
        "width REAL NOT NULL DEFAULT 100,"
        "height REAL NOT NULL DEFAULT 100,"
        "z_order REAL NOT NULL DEFAULT 0,"
        "common_style TEXT NOT NULL DEFAULT '{}',"
        "component_config TEXT NOT NULL DEFAULT '{}',"
        "schema_version INTEGER NOT NULL DEFAULT 1,"
        "FOREIGN KEY (page_id) REFERENCES dashboard_pages(id) ON DELETE CASCADE)",

        "CREATE TABLE IF NOT EXISTS alarm_rules ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "tag_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "alarm_type INTEGER NOT NULL,"
        "threshold REAL,"
        "delay_ms INTEGER NOT NULL DEFAULT 0,"
        "hysteresis REAL NOT NULL DEFAULT 0,"
        "severity INTEGER NOT NULL DEFAULT 0,"
        "enabled INTEGER NOT NULL DEFAULT 1,"
        "FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE)",

        "CREATE TABLE IF NOT EXISTS alarm_events ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "rule_id INTEGER NOT NULL,"
        "tag_id INTEGER NOT NULL,"
        "triggered_at TEXT NOT NULL,"
        "acknowledged_at TEXT,"
        "recovered_at TEXT,"
        "trigger_value REAL,"
        "FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE)",

        "CREATE TABLE IF NOT EXISTS history_samples ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "tag_id INTEGER NOT NULL,"
        "value REAL NOT NULL,"
        "quality INTEGER NOT NULL DEFAULT 0,"
        "sampled_at TEXT NOT NULL,"
        "FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE)",

        "CREATE INDEX IF NOT EXISTS idx_history_tag_time ON history_samples(tag_id, sampled_at)",

        "CREATE TABLE IF NOT EXISTS recipes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "description TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')))",

        "CREATE TABLE IF NOT EXISTS recipe_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "recipe_id INTEGER NOT NULL,"
        "tag_id INTEGER NOT NULL,"
        "value REAL NOT NULL,"
        "FOREIGN KEY (recipe_id) REFERENCES recipes(id) ON DELETE CASCADE,"
        "FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE)",

        "CREATE TABLE IF NOT EXISTS operation_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "tag_id INTEGER,"
        "old_value REAL,"
        "new_value REAL,"
        "success INTEGER NOT NULL,"
        "error_message TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')))",

        "CREATE TABLE IF NOT EXISTS app_settings ("
        "key TEXT PRIMARY KEY,"
        "value TEXT NOT NULL)",

        "INSERT INTO schema_migrations (version) VALUES (1)"
    };

    for (const auto& stmt : stmts) {
        QSqlQuery q(db);
        if (!q.exec(stmt)) {
            qWarning() << "Migration V1 failed:" << q.lastError().text() << "\nSQL:" << stmt;
            return false;
        }
    }

    return true;
}
