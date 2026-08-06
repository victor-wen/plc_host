#pragma once

#include <QString>

class AppContext {
public:
    static AppContext& instance();

    QString dbPath() const;

private:
    AppContext();
    QString m_dbPath;
};
