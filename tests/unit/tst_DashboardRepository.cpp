#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include "dashboard/DashboardDocument.h"
#include "dashboard/DashboardRepository.h"
#include "storage/DatabaseMigrator.h"

// 每个测试使用独立命名连接与独立 .db 文件（见 tst_DatabaseMigrations 惯例），
// 避免 QSqlDatabase 全局连接名的互相干扰。
class DashboardRepositoryTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tempDir;

    // 创建并迁移命名连接；连接保持注册（打开状态），测试结束负责 close+remove。
    // 失败时返回空字符串。
    QString setupDb(const QString& connectionName)
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(m_tempDir.path() + "/" + connectionName + ".db");
        if (!db.open()) {
            qWarning() << "setupDb open failed:" << db.lastError().text();
            return {};
        }
        {
            QSqlQuery q(db);
            q.exec("PRAGMA journal_mode=WAL");
            // 外键级联删除依赖该 pragma（per-connection）
            q.exec("PRAGMA foreign_keys=ON");
        }
        if (!DatabaseMigrator::migrate(db)) {
            qWarning() << "setupDb migrate failed";
            db.close();
            return {};
        }
        return connectionName;
    }

    void closeAndRemove(const QString& connectionName)
    {
        {
            QSqlDatabase db = QSqlDatabase::database(connectionName);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }

private slots:
    void createPage_loadPages_returnsCorrectPage()
    {
        const QString connName = "dash_create_page";
        QVERIFY(!setupDb(connName).isEmpty());

        {
            DashboardRepository repo(connName);
            DashboardPage page;
            page.name = "Overview";
            page.width = 1280;
            page.height = 720;
            page.background = "#123456";
            page.sortOrder = 3;

            QVERIFY(repo.savePage(page));
            QVERIFY(page.id > 0); // INSERT 后回填 id

            const auto pages = repo.loadPages();
            QCOMPARE(pages.size(), 1);
            QCOMPARE(pages[0].id, page.id);
            QCOMPARE(pages[0].name, QString("Overview"));
            QCOMPARE(pages[0].width, 1280);
            QCOMPARE(pages[0].height, 720);
            QCOMPARE(pages[0].background, QString("#123456"));
            QCOMPARE(pages[0].sortOrder, 3);
        }
        closeAndRemove(connName);
    }

    void updatePageName_reflectedOnReload()
    {
        const QString connName = "dash_update_page";
        QVERIFY(!setupDb(connName).isEmpty());

        {
            DashboardRepository repo(connName);
            DashboardPage page;
            page.name = "Old Name";
            page.width = 800;
            page.height = 600;
            page.sortOrder = 1;
            QVERIFY(repo.savePage(page));
            const int id = page.id;
            QVERIFY(id > 0);

            page.name = "New Name";
            page.sortOrder = 7;
            QVERIFY(repo.savePage(page));

            const auto pages = repo.loadPages();
            QCOMPARE(pages.size(), 1);
            QCOMPARE(pages[0].id, id); // 更新不改变 id
            QCOMPARE(pages[0].name, QString("New Name"));
            QCOMPARE(pages[0].width, 800); // 未改动字段保持
            QCOMPARE(pages[0].height, 600);
            QCOMPARE(pages[0].sortOrder, 7);
        }
        closeAndRemove(connName);
    }

    void deletePage_cascadesItems()
    {
        const QString connName = "dash_delete_page";
        QVERIFY(!setupDb(connName).isEmpty());

        {
            DashboardRepository repo(connName);
            DashboardPage page;
            page.name = "To Delete";
            QVERIFY(repo.savePage(page));

            QVector<DashboardItem> items;
            DashboardItem a;
            a.itemType = "rect";
            a.x = 10;
            a.y = 20;
            a.width = 200;
            a.height = 100;
            items.append(a);
            DashboardItem b;
            b.itemType = "text";
            b.x = 5;
            b.y = 5;
            items.append(b);
            QVERIFY(repo.saveItems(page.id, items));
            QCOMPARE(repo.loadItems(page.id).size(), 2);

            QVERIFY(repo.deletePage(page.id));

            QVERIFY(repo.loadPages().isEmpty()); // 页已删除
            QVERIFY(repo.loadItems(page.id).isEmpty()); // 级联删除 items
        }
        closeAndRemove(connName);
    }

    void saveItems_loadItems_roundtripAllFields()
    {
        const QString connName = "dash_items_roundtrip";
        QVERIFY(!setupDb(connName).isEmpty());

        {
            DashboardRepository repo(connName);
            DashboardPage page;
            page.name = "Items";
            QVERIFY(repo.savePage(page));

            QVector<DashboardItem> items;
            DashboardItem value;
            value.itemType = "value";
            value.x = 12.5;
            value.y = 34.25;
            value.width = 150.75;
            value.height = 60.5;
            value.zOrder = 2.0;
            value.schemaVersion = 2;
            items.append(value);
            DashboardItem led;
            led.itemType = "led";
            led.x = 1;
            led.y = 2;
            led.width = 40;
            led.height = 40;
            led.zOrder = 0;
            items.append(led);

            QVERIFY(repo.saveItems(page.id, items));

            const auto loaded = repo.loadItems(page.id);
            QCOMPARE(loaded.size(), 2);

            // loadItems 按 z_order 升序，此处按类型定位避免依赖顺序
            const DashboardItem* v = nullptr;
            const DashboardItem* l = nullptr;
            for (const auto& it : loaded) {
                if (it.itemType == "value")
                    v = &it;
                else if (it.itemType == "led")
                    l = &it;
            }
            QVERIFY(v != nullptr);
            QVERIFY(l != nullptr);

            QCOMPARE(v->pageId, page.id);
            QCOMPARE(v->itemType, QString("value"));
            QCOMPARE(v->x, 12.5);
            QCOMPARE(v->y, 34.25);
            QCOMPARE(v->width, 150.75);
            QCOMPARE(v->height, 60.5);
            QCOMPARE(v->zOrder, 2.0);
            QCOMPARE(v->schemaVersion, 2);

            QCOMPARE(l->pageId, page.id);
            QCOMPARE(l->itemType, QString("led"));
            QCOMPARE(l->x, 1.0);
            QCOMPARE(l->y, 2.0);
            QCOMPARE(l->width, 40.0);
            QCOMPARE(l->height, 40.0);
            QCOMPARE(l->zOrder, 0.0);
            QCOMPARE(l->schemaVersion, 1);
        }
        closeAndRemove(connName);
    }

    void zOrder_roundtrip_isExact()
    {
        const QString connName = "dash_zorder";
        QVERIFY(!setupDb(connName).isEmpty());

        {
            DashboardRepository repo(connName);
            DashboardPage page;
            page.name = "Z";
            QVERIFY(repo.savePage(page));

            QVector<DashboardItem> items;
            DashboardItem top;
            top.itemType = "rect";
            top.zOrder = 3.5;
            items.append(top);
            DashboardItem middle;
            middle.itemType = "rect";
            middle.zOrder = -1.25;
            items.append(middle);
            DashboardItem bottom;
            bottom.itemType = "rect";
            bottom.zOrder = 0.0;
            items.append(bottom);

            QVERIFY(repo.saveItems(page.id, items));

            const auto loaded = repo.loadItems(page.id);
            QCOMPARE(loaded.size(), 3);
            // loadItems 按 z_order 升序返回
            QCOMPARE(loaded[0].zOrder, -1.25);
            QCOMPARE(loaded[1].zOrder, 0.0);
            QCOMPARE(loaded[2].zOrder, 3.5);
        }
        closeAndRemove(connName);
    }

    void jsonFields_roundtrip_isExact()
    {
        const QString connName = "dash_json";
        QVERIFY(!setupDb(connName).isEmpty());

        {
            DashboardRepository repo(connName);
            DashboardPage page;
            page.name = "Json";
            QVERIFY(repo.savePage(page));

            QVector<DashboardItem> items;
            DashboardItem item;
            item.itemType = "button";
            item.commonStyle = QJsonObject{{"fillColor", "#ff8800"},
                                           {"borderWidth", 2},
                                           {"nested", QJsonObject{{"radius", 4}}}};
            item.config = QJsonObject{{"tagId", 42},
                                      {"min", 0},
                                      {"max", 100},
                                      {"action", QJsonObject{{"type", "toggle"}, {"onValue", 1}}}};
            items.append(item);

            QVERIFY(repo.saveItems(page.id, items));

            const auto loaded = repo.loadItems(page.id);
            QCOMPARE(loaded.size(), 1);
            QVERIFY(loaded[0].commonStyle == item.commonStyle);
            QVERIFY(loaded[0].config == item.config);
            QCOMPARE(loaded[0].config.value("tagId").toInt(), 42);
            QCOMPARE(loaded[0].config.value("action").toObject().value("type").toString(),
                     QString("toggle"));
        }
        closeAndRemove(connName);
    }
};

QTEST_MAIN(DashboardRepositoryTest)
#include "tst_DashboardRepository.moc"
