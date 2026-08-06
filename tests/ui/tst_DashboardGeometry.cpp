#include <QApplication>
#include <QMouseEvent>
#include <QTest>

#include "dashboard/DashboardBaseItem.h"
#include "dashboard/DashboardScene.h"
#include "dashboard/DashboardView.h"

namespace {

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

// 看板组件几何操作测试 (Phase 2 DASH-03)：多选拖动同步、最小尺寸缩放、
// 层级微调、复制/粘贴、删除与对齐。离屏渲染环境（见 main）。
class DashboardGeometryTest : public QObject {
    Q_OBJECT
private slots:
    void multiSelect();
    void resizeEnforcesMinimum();
    void stepForwardOrder();
    void copyPaste();
    void deleteReducesCount();
    void alignLeft();
};

void DashboardGeometryTest::multiSelect()
{
    DashboardScene scene;
    auto* a = scene.addItem(makeItem(QStringLiteral("rect"), 0, 0, 100, 50));
    auto* b = scene.addItem(makeItem(QStringLiteral("value"), 120, 60, 80, 40));
    a->setSelected(true);
    b->setSelected(true);

    // 模拟用户拖拽 a（使其成为鼠标抓取者）：其余选中项按相同 delta 跟随移动。
    // 程序化 setPos（非抓取者）不会触发同步。
    a->grabMouse();
    a->setPos(50, 30);
    a->ungrabMouse();

    QCOMPARE(a->pos(), QPointF(50, 30));
    QCOMPARE(b->pos(), QPointF(170, 90));

    // 两组件相对位置在同步移动后保持不变。
    QCOMPARE(b->pos() - a->pos(), QPointF(120, 60));
    QCOMPARE(a->pos() - b->pos(), QPointF(-120, -60));
}

void DashboardGeometryTest::resizeEnforcesMinimum()
{
    DashboardScene scene;
    auto* item = scene.addItem(makeItem(QStringLiteral("rect"), 0, 0, 100, 100));
    DashboardView view(&scene);
    view.resize(400, 300);
    view.show();
    QCoreApplication::processEvents();

    // 拖拽右下角缩放手柄：从组件右下角 (100,100) 拖到 scene (10,10)，
    // 目标尺寸 10x10 小于最小尺寸，应被钳制到 40x30。
    const QPointF br(item->boundingRect().width(), item->boundingRect().height());
    const QPoint start = view.mapFromScene(item->mapToScene(br));
    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, start);

    const QPoint end = view.mapFromScene(QPointF(10, 10));
    QMouseEvent move(QEvent::MouseMove, QPointF(end), view.viewport()->mapToGlobal(end),
                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &move);

    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, end);

    QCOMPARE(item->boundingRect().width(), DashboardBaseItem::kMinWidth);
    QCOMPARE(item->boundingRect().height(), DashboardBaseItem::kMinHeight);
    QVERIFY(item->boundingRect().width() >= DashboardBaseItem::kMinWidth);
    QVERIFY(item->boundingRect().height() >= DashboardBaseItem::kMinHeight);
    QCOMPARE(item->pos(), QPointF(0, 0)); // 右下角拖拽不改变左上角位置
}

void DashboardGeometryTest::stepForwardOrder()
{
    DashboardScene scene;
    auto* a = scene.addItem(makeItem(QStringLiteral("rect")));
    auto* b = scene.addItem(makeItem(QStringLiteral("value")));
    QCOMPARE(a->zValue(), 0);
    QCOMPARE(b->zValue(), 0);

    b->setSelected(true);
    scene.stepForward();
    QCOMPARE(b->zValue(), 1); // 层级上移 +1
    QCOMPARE(a->zValue(), 0); // 未选中项不受影响

    scene.stepBackward();
    QCOMPARE(b->zValue(), 0); // 层级下移回到原位
}

void DashboardGeometryTest::copyPaste()
{
    DashboardScene scene;
    auto* a = scene.addItem(makeItem(QStringLiteral("rect"), 10, 20, 100, 50));
    a->setSelected(true);

    scene.copySelected();
    QCOMPARE(scene.dashboardItems().size(), 1); // 复制不改变场景

    scene.pasteClipboard();
    QCOMPARE(scene.dashboardItems().size(), 2); // 粘贴后组件数 +1

    DashboardBaseItem* pasted = nullptr;
    for (auto* item : scene.dashboardItems())
        if (item != a)
            pasted = item;
    QVERIFY(pasted != nullptr);
    QCOMPARE(pasted->pos(), QPointF(30, 40)); // 原位置 +20px 偏移
    QCOMPARE(pasted->itemType, a->itemType);
    QVERIFY(pasted->isSelected()); // 粘贴后选中新组件
    QCOMPARE(scene.selectedItems().size(), 1);
}

void DashboardGeometryTest::deleteReducesCount()
{
    DashboardScene scene;
    scene.addItem(makeItem(QStringLiteral("rect")));
    auto* b = scene.addItem(makeItem(QStringLiteral("value")));
    scene.addItem(makeItem(QStringLiteral("led")));
    QCOMPARE(scene.dashboardItems().size(), 3);

    b->setSelected(true);
    scene.deleteSelected();

    QCOMPARE(scene.dashboardItems().size(), 2); // 删除后组件数 -1
    QCOMPARE(scene.selectedItems().size(), 0);
    QVERIFY(!scene.dashboardItems().contains(b));
}

void DashboardGeometryTest::alignLeft()
{
    DashboardScene scene;
    auto* a = scene.addItem(makeItem(QStringLiteral("rect"), 0, 0, 100, 50));
    auto* b = scene.addItem(makeItem(QStringLiteral("rect"), 80, 60, 60, 40));
    a->setSelected(true);
    b->setSelected(true);

    scene.alignSelected(Qt::AlignLeft);

    // 左对齐：所有选中项 x 坐标一致。
    QCOMPARE(a->pos().x(), 0.0);
    QCOMPARE(b->pos().x(), 0.0);
    QCOMPARE(a->pos().x(), b->pos().x());
}

int main(int argc, char* argv[])
{
    // 无显示环境下使用离屏平台运行 UI 测试。
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    DashboardGeometryTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_DashboardGeometry.moc"
