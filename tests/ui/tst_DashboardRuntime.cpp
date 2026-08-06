#include <QApplication>
#include <QDateTime>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include "dashboard/runtime/ButtonActionExecutor.h"
#include "dashboard/DashboardController.h"
#include "dashboard/DashboardRepository.h"
#include "dashboard/DashboardScene.h"
#include "dashboard/DashboardView.h"
#include "domain/Tag.h"
#include "runtime/AcquisitionEngine.h"
#include "runtime/TagCache.h"
#include "runtime/WriteQueue.h"
#include "storage/DatabaseMigrator.h"

namespace {

// 顶层项计数（缩放手柄为子项，不计入）：等价于组件数。
int topLevelItemCount(const QGraphicsScene& scene)
{
    int count = 0;
    const auto all = scene.items();
    for (QGraphicsItem* item : all) {
        if (!item->parentItem())
            ++count;
    }
    return count;
}

} // namespace

// 看板运行时协调器测试（Phase 2 DASH-09）：页面加载/切换与编辑/运行模式隔离。
// 点动释放通过真实 ButtonActionExecutor 的 writeRequested 信号（QSignalSpy）验证：
// 页面/模式切换确实把按住的点动按钮以 isRelease=true 写命令释放。
// 离屏渲染环境（见 main）。
class DashboardRuntimeTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tempDir;

    // 创建并迁移命名连接（每个测试独立连接名与独立 .db 文件，见
    // tst_DashboardRepository 惯例）。失败时返回空字符串。
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

    // 让执行器处于"在线且可写"状态：在线连接 + 可写 tag + 新鲜 Good 值缓存。
    void makeExecutorWriteable(ButtonActionExecutor& executor, TagCache& cache)
    {
        Tag tag;
        tag.id = 1;
        tag.name = QStringLiteral("pump");
        tag.readOnly = false;
        executor.setTagCache(&cache);
        executor.setTags({tag});
        executor.setConnectionState(ConnectionState::Online);

        TagValue tv;
        tv.tagId = 1;
        tv.value = 0;
        tv.quality = Quality::Good;
        tv.timestamp = QDateTime::currentDateTime();
        cache.updateValues(QHash<int, TagValue>{{1, tv}});
    }

    // 构造一个"按住点动按钮"的动作：paramA=1 按下值，paramB=0 松开值。
    ButtonAction momentaryPressAction() const
    {
        ButtonAction action;
        action.type = ButtonActionType::Momentary;
        action.paramA = 1;
        action.paramB = 0;
        return action;
    }

private slots:
    void switchToRunMode_locksSceneItems()
    {
        const QString connName = "dash_runtime_lock";
        QVERIFY(!setupDb(connName).isEmpty());

        DashboardScene scene;
        DashboardView view(&scene);
        DashboardRepository repo(connName);
        ButtonActionExecutor executor;
        DashboardController controller(&scene, &view, &repo, &executor);

        // 预置一页 + 一个组件，加载后组件处于编辑模式（可移动/可选中）。
        DashboardPage page;
        page.name = "Run Mode";
        QVERIFY(repo.savePage(page));
        QVector<DashboardItem> items;
        DashboardItem meta;
        meta.itemType = "rect";
        items.append(meta);
        QVERIFY(repo.saveItems(page.id, items));

        controller.loadPage(page.id);
        QCOMPARE(scene.dashboardItems().size(), 1);
        auto* item = scene.dashboardItems().constFirst();
        QVERIFY(scene.isEditMode());
        QVERIFY(item->flags().testFlag(QGraphicsItem::ItemIsMovable));

        // 编辑 → 运行：场景退出编辑模式，全部组件锁定，视图关闭拖拽。
        controller.setEditMode(false);
        QVERIFY(!controller.isEditMode());
        QVERIFY(!scene.isEditMode());
        QCOMPARE(view.dragMode(), QGraphicsView::NoDrag);
        for (auto* it : scene.dashboardItems()) {
            QVERIFY(!it->flags().testFlag(QGraphicsItem::ItemIsMovable));
            QVERIFY(!it->flags().testFlag(QGraphicsItem::ItemIsSelectable));
        }

        // 恢复编辑模式：组件重新可移动/可选中，视图恢复橡皮筋多选。
        controller.setEditMode(true);
        QVERIFY(controller.isEditMode());
        QVERIFY(scene.isEditMode());
        QCOMPARE(view.dragMode(), QGraphicsView::RubberBandDrag);
        QVERIFY(item->flags().testFlag(QGraphicsItem::ItemIsMovable));
        QVERIFY(item->flags().testFlag(QGraphicsItem::ItemIsSelectable));

        closeAndRemove(connName);
    }

    void setEditMode_releasesAllMomentary()
    {
        const QString connName = "dash_mode_release";
        QVERIFY(!setupDb(connName).isEmpty());

        DashboardScene scene;
        DashboardView view(&scene);
        DashboardRepository repo(connName);
        ButtonActionExecutor executor;
        TagCache cache;
        makeExecutorWriteable(executor, cache);
        DashboardController controller(&scene, &view, &repo, &executor);

        // 按住一个点动按钮（按下写命令不计入后续 spy）。
        executor.execute(momentaryPressAction(), 1);
        QSignalSpy writeSpy(&executor, &ButtonActionExecutor::writeRequested);

        // 编辑 → 运行：模式切换必须释放全部点动（isRelease=true 写命令）。
        controller.setEditMode(false);
        QCOMPARE(writeSpy.count(), 1);
        const WriteCommand release = writeSpy.at(0).at(0).value<WriteCommand>();
        QVERIFY(release.isRelease);
        QCOMPARE(release.priority, 1); // 点动释放强制高优先级

        // 再次按住点动，运行 → 编辑：同样释放（离开运行模式的点动不再安全）。
        executor.execute(momentaryPressAction(), 1);
        QSignalSpy secondSpy(&executor, &ButtonActionExecutor::writeRequested);
        controller.setEditMode(true);
        QCOMPARE(secondSpy.count(), 1);
        QVERIFY(secondSpy.at(0).at(0).value<WriteCommand>().isRelease);

        closeAndRemove(connName);
    }

    void switchPage_releasesAllMomentary()
    {
        const QString connName = "dash_page_release";
        QVERIFY(!setupDb(connName).isEmpty());

        DashboardScene scene;
        DashboardView view(&scene);
        DashboardRepository repo(connName);
        ButtonActionExecutor executor;
        TagCache cache;
        makeExecutorWriteable(executor, cache);
        DashboardController controller(&scene, &view, &repo, &executor);

        DashboardPage pageA;
        pageA.name = "Page A";
        QVERIFY(repo.savePage(pageA));
        DashboardPage pageB;
        pageB.name = "Page B";
        QVERIFY(repo.savePage(pageB));

        // 按住点动后切换到 A 页：切页释放点动 + 加载目标页。
        executor.execute(momentaryPressAction(), 1);
        QSignalSpy writeSpy(&executor, &ButtonActionExecutor::writeRequested);
        controller.switchPage(pageA.id);
        QCOMPARE(writeSpy.count(), 1);
        QVERIFY(writeSpy.at(0).at(0).value<WriteCommand>().isRelease);
        QCOMPARE(controller.currentPageId(), pageA.id);

        // 切换到同一页：无操作（不重复释放、不重复加载）。
        controller.switchPage(pageA.id);
        QCOMPARE(writeSpy.count(), 1);

        // 再次按住点动，切换到另一页：再次释放。
        executor.execute(momentaryPressAction(), 1);
        QSignalSpy secondSpy(&executor, &ButtonActionExecutor::writeRequested);
        controller.switchPage(pageB.id);
        QCOMPARE(secondSpy.count(), 1);
        QVERIFY(secondSpy.at(0).at(0).value<WriteCommand>().isRelease);
        QCOMPARE(controller.currentPageId(), pageB.id);

        closeAndRemove(connName);
    }

    void loadPage_displaysItems()
    {
        const QString connName = "dash_load_items";
        QVERIFY(!setupDb(connName).isEmpty());

        DashboardScene scene;
        DashboardView view(&scene);
        DashboardRepository repo(connName);
        ButtonActionExecutor executor;
        DashboardController controller(&scene, &view, &repo, &executor);

        DashboardPage page;
        page.name = "Overview";
        page.width = 1280;
        page.height = 720;
        QVERIFY(repo.savePage(page));

        QVector<DashboardItem> items;
        DashboardItem rect;
        rect.itemType = "rect";
        rect.x = 10;
        rect.y = 20;
        rect.width = 200;
        rect.height = 100;
        items.append(rect);
        DashboardItem value;
        value.itemType = "value";
        value.x = 300;
        value.y = 50;
        value.width = 150;
        value.height = 60;
        items.append(value);
        QVERIFY(repo.saveItems(page.id, items));

        controller.loadPage(page.id);

        QCOMPARE(controller.currentPageId(), page.id);
        QCOMPARE(scene.dashboardItems().size(), 2);
        QCOMPARE(topLevelItemCount(scene), 2);
        QCOMPARE(scene.sceneRect(), QRectF(0, 0, 1280, 720));

        DashboardBaseItem* rectItem = nullptr;
        DashboardBaseItem* valueItem = nullptr;
        for (auto* it : scene.dashboardItems()) {
            if (it->itemType == "rect")
                rectItem = it;
            else if (it->itemType == "value")
                valueItem = it;
        }
        QVERIFY(rectItem != nullptr);
        QVERIFY(valueItem != nullptr);
        QCOMPARE(rectItem->pos(), QPointF(10, 20));
        QCOMPARE(rectItem->boundingRect().size(), QSizeF(200, 100));
        QCOMPARE(valueItem->pos(), QPointF(300, 50));
        QCOMPARE(valueItem->boundingRect().size(), QSizeF(150, 60));

        // 再次加载同一页：先清空场景再重建，组件不重复。
        controller.loadPage(page.id);
        QCOMPARE(scene.dashboardItems().size(), 2);

        closeAndRemove(connName);
    }

    void navigatePageAction_switchesPage()
    {
        const QString connName = "dash_nav";
        QVERIFY(!setupDb(connName).isEmpty());

        DashboardScene scene;
        DashboardView view(&scene);
        DashboardRepository repo(connName);
        ButtonActionExecutor executor; // 真实执行器：NavigatePage → pageNavigationRequested
        DashboardController controller(&scene, &view, &repo, &executor);

        DashboardPage pageA;
        pageA.name = "A";
        QVERIFY(repo.savePage(pageA));
        DashboardPage pageB;
        pageB.name = "B";
        QVERIFY(repo.savePage(pageB));

        controller.switchPage(pageA.id);
        QCOMPARE(controller.currentPageId(), pageA.id);

        // 运行模式按钮的 NavigatePage 动作：执行器发出 pageNavigationRequested，
        // 控制器连接后完成跨页跳转。
        ButtonAction action;
        action.type = ButtonActionType::NavigatePage;
        action.targetPageId = pageB.id;
        executor.execute(action, -1);

        QCOMPARE(controller.currentPageId(), pageB.id);

        closeAndRemove(connName);
    }
};

int main(int argc, char* argv[])
{
    // 无显示环境下使用离屏平台运行 UI 测试。
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    qRegisterMetaType<WriteCommand>();
    DashboardRuntimeTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_DashboardRuntime.moc"
