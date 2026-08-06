#include "DashboardRepository.h"

#include <QDebug>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {

// 返回命名连接的一个句柄；连接未打开时记录警告并返回无效句柄。
QSqlDatabase dbFor(const QString& connectionName)
{
    QSqlDatabase db = QSqlDatabase::database(connectionName);
    if (!db.isOpen())
        qWarning() << "DashboardRepository: database connection '" << connectionName
                   << "' is not open";
    return db;
}

QString jsonToText(const QJsonObject& obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// 解析失败（损坏 JSON）时返回空对象，不视为错误，由上层按占位处理。
QJsonObject textToJson(const QVariant& text)
{
    return QJsonDocument::fromJson(text.toString().toUtf8()).object();
}

// 默认构造的 QString() 的 isNull() 为 true，SQLite 驱动会将其绑定为 NULL；
// NOT NULL 列需要显式归一化为非 null 的空字符串。
QString notNull(const QString& s)
{
    return s.isEmpty() ? QStringLiteral("") : s;
}

} // namespace

DashboardRepository::DashboardRepository(const QString& connectionName, QObject* parent)
    : QObject(parent)
    , m_connectionName(connectionName)
{
}

QVector<DashboardPage> DashboardRepository::loadPages()
{
    QVector<DashboardPage> pages;
    QSqlDatabase db = dbFor(m_connectionName);
    if (!db.isOpen())
        return pages;

    QSqlQuery q(db);
    if (!q.exec("SELECT id, name, width, height, background, sort_order "
                "FROM dashboard_pages ORDER BY sort_order, id")) {
        qWarning() << "DashboardRepository::loadPages failed:" << q.lastError().text();
        return pages;
    }

    while (q.next()) {
        DashboardPage page;
        page.id = q.value(0).toInt();
        page.name = q.value(1).toString();
        page.width = q.value(2).toInt();
        page.height = q.value(3).toInt();
        page.background = q.value(4).toString();
        page.sortOrder = q.value(5).toInt();
        pages.append(page);
    }
    return pages;
}

bool DashboardRepository::savePage(DashboardPage& page)
{
    QSqlDatabase db = dbFor(m_connectionName);
    if (!db.isOpen())
        return false;

    if (page.id == -1) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO dashboard_pages (name, width, height, background, sort_order) "
                  "VALUES (:name, :width, :height, :background, :sortOrder)");
        q.bindValue(":name", notNull(page.name));
        q.bindValue(":width", page.width);
        q.bindValue(":height", page.height);
        q.bindValue(":background", notNull(page.background));
        q.bindValue(":sortOrder", page.sortOrder);
        if (!q.exec()) {
            qWarning() << "DashboardRepository::savePage INSERT failed:" << q.lastError().text();
            return false;
        }
        page.id = q.lastInsertId().toInt();
        return true;
    }

    QSqlQuery q(db);
    q.prepare("UPDATE dashboard_pages SET name = :name, width = :width, height = :height, "
              "background = :background, sort_order = :sortOrder WHERE id = :id");
    q.bindValue(":name", notNull(page.name));
    q.bindValue(":width", page.width);
    q.bindValue(":height", page.height);
    q.bindValue(":background", notNull(page.background));
    q.bindValue(":sortOrder", page.sortOrder);
    q.bindValue(":id", page.id);
    if (!q.exec()) {
        qWarning() << "DashboardRepository::savePage UPDATE failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool DashboardRepository::deletePage(int pageId)
{
    QSqlDatabase db = dbFor(m_connectionName);
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare("DELETE FROM dashboard_pages WHERE id = :id");
    q.bindValue(":id", pageId);
    if (!q.exec()) {
        qWarning() << "DashboardRepository::deletePage failed:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

QVector<DashboardItem> DashboardRepository::loadItems(int pageId)
{
    QVector<DashboardItem> items;
    QSqlDatabase db = dbFor(m_connectionName);
    if (!db.isOpen())
        return items;

    QSqlQuery q(db);
    q.prepare("SELECT id, page_id, item_type, x, y, width, height, z_order, "
              "common_style, component_config, schema_version "
              "FROM dashboard_items WHERE page_id = :pageId ORDER BY z_order, id");
    q.bindValue(":pageId", pageId);
    if (!q.exec()) {
        qWarning() << "DashboardRepository::loadItems failed:" << q.lastError().text();
        return items;
    }

    while (q.next()) {
        DashboardItem item;
        item.id = q.value(0).toInt();
        item.pageId = q.value(1).toInt();
        item.itemType = q.value(2).toString();
        item.x = q.value(3).toReal();
        item.y = q.value(4).toReal();
        item.width = q.value(5).toReal();
        item.height = q.value(6).toReal();
        item.zOrder = q.value(7).toReal();
        item.commonStyle = textToJson(q.value(8));
        item.config = textToJson(q.value(9));
        item.schemaVersion = q.value(10).toInt();
        items.append(item);
    }
    return items;
}

bool DashboardRepository::saveItems(int pageId, const QVector<DashboardItem>& items)
{
    QSqlDatabase db = dbFor(m_connectionName);
    if (!db.isOpen())
        return false;

    if (!db.transaction()) {
        qWarning() << "DashboardRepository::saveItems begin transaction failed:"
                   << db.lastError().text();
        return false;
    }

    QSqlQuery del(db);
    del.prepare("DELETE FROM dashboard_items WHERE page_id = :pageId");
    del.bindValue(":pageId", pageId);
    if (!del.exec()) {
        qWarning() << "DashboardRepository::saveItems DELETE failed:" << del.lastError().text();
        db.rollback();
        return false;
    }

    for (const auto& item : items) {
        QSqlQuery ins(db);
        ins.prepare("INSERT INTO dashboard_items "
                    "(page_id, item_type, x, y, width, height, z_order, "
                    "common_style, component_config, schema_version) "
                    "VALUES (:pageId, :itemType, :x, :y, :width, :height, :zOrder, "
                    ":commonStyle, :componentConfig, :schemaVersion)");
        ins.bindValue(":pageId", pageId);
        ins.bindValue(":itemType", notNull(item.itemType));
        ins.bindValue(":x", item.x);
        ins.bindValue(":y", item.y);
        ins.bindValue(":width", item.width);
        ins.bindValue(":height", item.height);
        ins.bindValue(":zOrder", item.zOrder);
        ins.bindValue(":commonStyle", jsonToText(item.commonStyle));
        ins.bindValue(":componentConfig", jsonToText(item.config));
        ins.bindValue(":schemaVersion", item.schemaVersion);
        if (!ins.exec()) {
            qWarning() << "DashboardRepository::saveItems INSERT failed:"
                       << ins.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        qWarning() << "DashboardRepository::saveItems commit failed:" << db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}
