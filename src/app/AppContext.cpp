#include "AppContext.h"
#include <QStandardPaths>
#include <QDir>

AppContext& AppContext::instance()
{
    static AppContext ctx;
    return ctx;
}

AppContext::AppContext()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    m_dbPath = dataDir + "/plc_host.db";
}

QString AppContext::dbPath() const
{
    return m_dbPath;
}
