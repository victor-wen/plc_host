#pragma once

#include <QSqlDatabase>

class DatabaseMigrator {
public:
    static bool migrate(QSqlDatabase& db);
    static int currentVersion(QSqlDatabase& db);

private:
    static bool migrateV1(QSqlDatabase& db);
};
