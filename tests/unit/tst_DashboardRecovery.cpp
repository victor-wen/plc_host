#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QUndoStack>

#include "dashboard/DashboardController.h"
#include "dashboard/DashboardRepository.h"
#include "dashboard/DashboardScene.h"
#include "dashboard/DashboardView.h"
#include "dashboard/DashboardDocument.h"
#include "dashboard/DashboardBaseItem.h"
#include "dashboard/runtime/ButtonActionExecutor.h"
#include "storage/DatabaseMigrator.h"

class DashboardRecoveryTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tempDir;

    struct Context {
        QSqlDatabase db;
        DashboardScene* scene = nullptr;
        DashboardView* view = nullptr;
        DashboardRepository* repo = nullptr;
        ButtonActionExecutor* executor = nullptr;
        DashboardController* ctrl = nullptr;

        void destroy() {
            delete ctrl;
            delete executor;
            delete repo;
            delete view;
            delete scene;
            db.close();
        }
    };

    Context createContext() {
        Context ctx;
        QString path = m_tempDir.path() + "/test.db";
        ctx.db = QSqlDatabase::addDatabase("QSQLITE", "dash_recovery");
        ctx.db.setDatabaseName(path);
        if (!ctx.db.open())
            return ctx;
        QSqlQuery q(ctx.db);
        q.exec("PRAGMA journal_mode=WAL");
        q.finish();

        DatabaseMigrator::migrate(ctx.db);

        ctx.scene = new DashboardScene;
        ctx.view = new DashboardView(ctx.scene);
        ctx.repo = new DashboardRepository("dash_recovery");
        ctx.executor = new ButtonActionExecutor;
        ctx.ctrl = new DashboardController(ctx.scene, ctx.view, ctx.repo, ctx.executor);
        return ctx;
    }

    void cleanup() {
        QSqlDatabase::removeDatabase("dash_recovery");
    }

private slots:
    void save_makesIsDirty_false()
    {
        auto ctx = createContext();
        int pageId = ctx.ctrl->createPage("Test");
        QVERIFY(pageId > 0);
        ctx.ctrl->loadPage(pageId);

        QVERIFY(!ctx.ctrl->isDirty());
        ctx.ctrl->save();
        QVERIFY(!ctx.ctrl->isDirty());

        ctx.destroy();
    }

    void corrupted_item_type_creates_placeholder()
    {
        auto ctx = createContext();
        int pageId = ctx.ctrl->createPage("Test");
        QVERIFY(pageId > 0);

        DashboardItem badItem;
        badItem.itemType = "unknown_xyz";
        badItem.pageId = pageId;
        badItem.width = 100;
        badItem.height = 100;
        ctx.repo->saveItems(pageId, {badItem});

        ctx.ctrl->loadPage(pageId);
        auto items = ctx.scene->dashboardItems();
        QVERIFY(!items.isEmpty());
        QCOMPARE(items.first()->itemType, QString("unknown_xyz"));

        ctx.destroy();
    }

    void save_then_reload_preserves_items()
    {
        auto ctx = createContext();
        int pageId = ctx.ctrl->createPage("Test");
        QVERIFY(pageId > 0);
        ctx.ctrl->loadPage(pageId);

        DashboardItem item;
        item.itemType = "rect";
        item.pageId = pageId;
        item.width = 100;
        item.height = 100;
        item.x = 50;
        item.y = 50;
        ctx.scene->addItem(item);

        QVERIFY(ctx.ctrl->save());

        ctx.ctrl->loadPage(pageId);
        auto items = ctx.scene->dashboardItems();
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0]->itemType, QString("rect"));

        ctx.destroy();
    }

    void draft_save_and_restore()
    {
        auto ctx = createContext();
        int pageId = ctx.ctrl->createPage("Test");
        QVERIFY(pageId > 0);
        ctx.ctrl->loadPage(pageId);

        DashboardItem item;
        item.itemType = "text";
        item.pageId = pageId;
        item.width = 120;
        item.height = 40;
        ctx.scene->addItem(item);

        ctx.ctrl->saveDraft();
        ctx.scene->clear();
        QVERIFY(ctx.scene->dashboardItems().isEmpty());

        QVERIFY(ctx.ctrl->restoreDraft());
        QCOMPARE(ctx.scene->dashboardItems().size(), 1);

        ctx.destroy();
    }

    void corrupted_item_does_not_block_others()
    {
        auto ctx = createContext();
        int pageId = ctx.ctrl->createPage("Test");
        QVERIFY(pageId > 0);

        DashboardItem good;
        good.itemType = "rect";
        good.pageId = pageId;
        good.width = 100;
        good.height = 100;
        DashboardItem bad;
        bad.itemType = "unknown_type";
        bad.pageId = pageId;
        bad.width = 100;
        bad.height = 100;
        ctx.repo->saveItems(pageId, {good, bad});

        ctx.ctrl->loadPage(pageId);
        auto items = ctx.scene->dashboardItems();
        QCOMPARE(items.size(), 2);

        ctx.destroy();
    }
};

QTEST_MAIN(DashboardRecoveryTest)
#include "tst_DashboardRecovery.moc"
