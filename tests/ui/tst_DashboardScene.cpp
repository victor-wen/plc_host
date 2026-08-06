#include <QApplication>
#include <QGraphicsRectItem>
#include <QTest>

#include "dashboard/DashboardBaseItem.h"
#include "dashboard/DashboardScene.h"
#include "dashboard/DashboardView.h"

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

// 测试辅助：构造一个未保存（id=-1）的组件元数据。
DashboardItem makeItem(const QString& type, qreal x = 0, qreal y = 0,
                       qreal width = 100, qreal height = 100)
{
    DashboardItem meta;
    meta.itemType = type;
    meta.x = x;
    meta.y = y;
    meta.width = width;
    meta.height = height;
    return meta;
}

} // namespace

// 看板自由画布测试（Phase 2 DASH-02）：组件工厂、编辑/运行模式、网格吸附、
// 层级、对齐、删除与视图模式切换。离屏渲染环境（见 main）。
class DashboardSceneTest : public QObject {
    Q_OBJECT
private slots:
    void addItem_creates_components()
    {
        DashboardScene scene;
        scene.addItem(makeItem(QStringLiteral("value")));
        scene.addItem(makeItem(QStringLiteral("rect")));
        scene.addItem(makeItem(QStringLiteral("unknownType")));

        QCOMPARE(scene.dashboardItems().size(), 3);
        // 顶层项 = 组件数（DASH-03 缩放手柄为子项，不计入顶层）。
        QCOMPARE(topLevelItemCount(scene), 3);

        QStringList types;
        for (auto* item : scene.dashboardItems())
            types << item->itemType;
        QVERIFY(types.contains(QStringLiteral("value")));
        QVERIFY(types.contains(QStringLiteral("rect")));

        // 工厂应用几何：位置、尺寸、zOrder 与元数据。
        auto* item = scene.dashboardItems().constFirst();
        QVERIFY(item != nullptr);
        QCOMPARE(item->boundingRect().size(), QSizeF(100, 100));
        QCOMPARE(item->pos(), QPointF(0, 0));
    }

    void setEditMode_locks_items()
    {
        DashboardScene scene;
        auto* value = scene.addItem(makeItem(QStringLiteral("value")));
        scene.addItem(makeItem(QStringLiteral("led")));

        // 默认编辑模式：可移动/可选中。
        QVERIFY(value->isEditMode());
        QVERIFY(value->flags().testFlag(QGraphicsItem::ItemIsMovable));
        QVERIFY(value->flags().testFlag(QGraphicsItem::ItemIsSelectable));

        // 运行模式：全部组件锁定。
        scene.setEditMode(false);
        QVERIFY(!value->isEditMode());
        for (auto* item : scene.dashboardItems()) {
            QVERIFY(!item->flags().testFlag(QGraphicsItem::ItemIsMovable));
            QVERIFY(!item->flags().testFlag(QGraphicsItem::ItemIsSelectable));
        }

        // 恢复编辑模式。
        scene.setEditMode(true);
        for (auto* item : scene.dashboardItems()) {
            QVERIFY(item->flags().testFlag(QGraphicsItem::ItemIsMovable));
            QVERIFY(item->flags().testFlag(QGraphicsItem::ItemIsSelectable));
        }
    }

    void snapToGrid_rounds_to_nearest()
    {
        DashboardScene scene;
        QCOMPARE(scene.snapToGrid(QPointF(13, 13)), QPointF(10, 10));
        QCOMPARE(scene.snapToGrid(QPointF(17.4, 22.6)), QPointF(20, 20));
        QCOMPARE(scene.snapToGrid(QPointF(-4, 6)), QPointF(0, 10));
        // 自定义网格尺寸。
        QCOMPARE(scene.snapToGrid(QPointF(25, 30), 20), QPointF(20, 40));
        // gridSize <= 0 视为不吸附。
        QCOMPARE(scene.snapToGrid(QPointF(13, 13), 0), QPointF(13, 13));
    }

    void bringToFront_sendToBack_adjusts_zvalue()
    {
        DashboardScene scene;
        auto* a = scene.addItem(makeItem(QStringLiteral("rect")));
        auto* b = scene.addItem(makeItem(QStringLiteral("value")));
        QCOMPARE(a->zValue(), 0);
        QCOMPARE(b->zValue(), 0);

        a->setSelected(true);
        scene.bringToFront();
        QVERIFY(a->zValue() > b->zValue());
        for (auto* item : scene.dashboardItems())
            QVERIFY(item->zValue() <= a->zValue());

        scene.clearSelection();
        b->setSelected(true);
        scene.sendToBack();
        QVERIFY(b->zValue() < a->zValue());
    }

    void alignSelected_uses_bounding_box()
    {
        DashboardScene scene;
        auto* a = scene.addItem(makeItem(QStringLiteral("rect"), 0, 0, 100, 50));
        auto* b = scene.addItem(makeItem(QStringLiteral("rect"), 50, 80, 100, 50));
        a->setSelected(true);
        b->setSelected(true);

        // 左对齐 + 顶对齐：全部贴包围盒左上角。
        scene.alignSelected(Qt::AlignLeft | Qt::AlignTop);
        QCOMPARE(a->pos(), QPointF(0, 0));
        QCOMPARE(b->pos(), QPointF(0, 0));

        // 水平居中：包围盒 x:[0,150] 中心 75，两组件宽 100 → x=25。
        a->setPos(0, 0);
        b->setPos(50, 80);
        scene.alignSelected(Qt::AlignHCenter);
        QCOMPARE(a->pos(), QPointF(25, 0));
        QCOMPARE(b->pos(), QPointF(25, 80));
    }

    void deleteSelected_removes_items()
    {
        DashboardScene scene;
        auto* a = scene.addItem(makeItem(QStringLiteral("rect")));
        scene.addItem(makeItem(QStringLiteral("value")));
        scene.addItem(makeItem(QStringLiteral("led")));
        QCOMPARE(scene.dashboardItems().size(), 3);

        a->setSelected(true);
        scene.deleteSelected();

        QCOMPARE(scene.dashboardItems().size(), 2);
        // 顶层项 = 组件数（缩放手柄为子项，不计入顶层）。
        QCOMPARE(topLevelItemCount(scene), 2);
        QCOMPARE(scene.selectedItems().size(), 0);
    }

    void selectedItems_filters_non_dashboard_items()
    {
        DashboardScene scene;
        auto* dbi = scene.addItem(makeItem(QStringLiteral("rect")));
        auto* foreign = new QGraphicsRectItem(0, 0, 50, 50);
        foreign->setFlag(QGraphicsItem::ItemIsSelectable, true); // 默认无交互 flags
        scene.addItem(foreign); // 非看板组件（using 引入的基类重载）

        dbi->setSelected(true);
        foreign->setSelected(true);
        QCOMPARE(scene.QGraphicsScene::selectedItems().size(), 2);
        QCOMPARE(scene.selectedItems().size(), 1);
        QCOMPARE(scene.selectedItems().constFirst(), dbi);
    }

    void setPage_sets_canvas_and_background()
    {
        DashboardScene scene;
        DashboardPage page;
        page.width = 800;
        page.height = 600;
        page.background = QStringLiteral("#123456");
        scene.setPage(page);

        QCOMPARE(scene.sceneRect(), QRectF(0, 0, 800, 600));
        QCOMPARE(scene.backgroundBrush().color(), QColor(QStringLiteral("#123456")));
        // 组件数 <100 使用线性索引。
        QCOMPARE(scene.itemIndexMethod(), QGraphicsScene::NoIndex);

        // 空背景回退默认深色，不崩溃。
        DashboardPage empty;
        empty.width = 1920;
        empty.height = 1080;
        scene.setPage(empty);
        QCOMPARE(scene.sceneRect(), QRectF(0, 0, 1920, 1080));
        QVERIFY(scene.backgroundBrush().color().isValid());
    }

    void view_edit_mode_and_fitToScreen()
    {
        DashboardScene scene;
        DashboardPage page;
        page.width = 800;
        page.height = 600;
        scene.setPage(page);
        scene.addItem(makeItem(QStringLiteral("value"), 100, 100, 200, 100));

        DashboardView view(&scene);

        view.setEditMode(true);
        QCOMPARE(view.dragMode(), QGraphicsView::RubberBandDrag);
        view.setEditMode(false);
        QCOMPARE(view.dragMode(), QGraphicsView::NoDrag);

        // fitToScreen：场景矩形完整适配视口。Qt fitInView 内建 2px 边距，
        // 缩放因子应接近无边距的理论最佳值 min(vw/800, vh/600)。
        view.resize(400, 300);
        view.show();
        QCoreApplication::processEvents();
        view.fitToScreen();

        QVERIFY(view.zoomFactor() > 0);
        const QRectF mapped = view.mapFromScene(scene.sceneRect()).boundingRect();
        QVERIFY(mapped.width() <= view.viewport()->width());
        QVERIFY(mapped.height() <= view.viewport()->height());

        const qreal ideal = qMin(static_cast<qreal>(view.viewport()->width()) / 800.0,
                                 static_cast<qreal>(view.viewport()->height()) / 600.0);
        QVERIFY(qAbs(view.zoomFactor() - ideal) < 0.05);
    }
};

int main(int argc, char* argv[])
{
    // 无显示环境下使用离屏平台运行 UI 测试。
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    DashboardSceneTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_DashboardScene.moc"
