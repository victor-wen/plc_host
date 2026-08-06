#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QTest>
#include <QUndoStack>

#include "dashboard/DashboardScene.h"
#include "dashboard/commands/AddItemCommand.h"
#include "dashboard/commands/MoveCommand.h"
#include "dashboard/commands/PropertyChangeCommand.h"
#include "dashboard/commands/RemoveItemCommand.h"
#include "dashboard/commands/ResizeCommand.h"

namespace {

// 测试辅助：构造未保存（id=-1）的组件元数据。
DashboardItem makeMeta(const QString& type = QStringLiteral("rect"))
{
    DashboardItem meta;
    meta.itemType = type;
    return meta;
}

} // namespace

// 撤销/重做命令测试 (Phase 2 DASH-04)：添加/删除/移动/缩放/属性修改命令的
// undo/redo 语义与 MoveCommand/ResizeCommand 的合并行为。
class UndoCommandsTest : public QObject {
    Q_OBJECT
private slots:
    void addItemCommand_undo_removesItemFromScene();
    void addItemCommand_redoAfterUndo_reusesSameItem();
    void addItemWithUndo_roundTrips();

    void removeItemCommand_undo_restoresAllProperties();
    void removeItemCommand_redoAfterUndo_recreatesItem();
    void deleteSelected_undo_restoresItems();

    void moveCommand_mergesConsecutiveMoves();
    void moveCommand_microMove_doesNotMerge();
    void multipleMoves_undo_returnsToInitialPosition();

    void resizeCommand_redoUndo_appliesRects();
    void resizeCommand_mergesConsecutiveResizes();

    void propertyChangeCommand_undo_restoresOldValue();
    void propertyChangeCommand_undefinedValue_removesProperty();
    void propertyChangeCommand_commonStyle_roundTrips();
};

void UndoCommandsTest::addItemCommand_undo_removesItemFromScene()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* cmd = new AddItemCommand(&scene, makeMeta(QStringLiteral("value")));

    stack.push(cmd); // push 即执行 redo
    QCOMPARE(scene.dashboardItems().size(), 1);
    QVERIFY(cmd->item() != nullptr);

    stack.undo();
    QCOMPARE(scene.dashboardItems().size(), 0);
    QVERIFY(!scene.items().contains(cmd->item()));
}

void UndoCommandsTest::addItemCommand_redoAfterUndo_reusesSameItem()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* cmd = new AddItemCommand(&scene, makeMeta());
    stack.push(cmd);
    DashboardBaseItem* created = cmd->item();

    stack.undo();
    stack.redo();

    QCOMPARE(scene.dashboardItems().size(), 1);
    QCOMPARE(cmd->item(), created); // 复用同一组件，无重复构造
}

void UndoCommandsTest::addItemWithUndo_roundTrips()
{
    DashboardScene scene;
    auto* item = scene.addItemWithUndo(makeMeta());
    QVERIFY(item != nullptr);
    QCOMPARE(scene.dashboardItems().size(), 1);

    scene.undoStack()->undo();
    QCOMPARE(scene.dashboardItems().size(), 0);

    scene.undoStack()->redo();
    QCOMPARE(scene.dashboardItems().size(), 1);
}

void UndoCommandsTest::removeItemCommand_undo_restoresAllProperties()
{
    DashboardScene scene;
    QUndoStack stack;

    DashboardItem meta = makeMeta(QStringLiteral("value"));
    meta.id = 7;
    meta.x = 12.5;
    meta.y = 34.25;
    meta.width = 150;
    meta.height = 60;
    meta.zOrder = 2.5;
    meta.schemaVersion = 2;
    meta.commonStyle = QJsonObject{{QStringLiteral("fillColor"), QStringLiteral("#ff8800")}};
    meta.config = QJsonObject{{QStringLiteral("tagId"), 42}};

    auto* item = scene.addItem(meta); // 原始工厂，不经撤销栈
    stack.push(new RemoveItemCommand(&scene, item, dashboardItemToJson(item)));

    QCOMPARE(scene.dashboardItems().size(), 0); // redo 已移除

    stack.undo();
    QCOMPARE(scene.dashboardItems().size(), 1);

    auto* restored = scene.dashboardItems().constFirst();
    QCOMPARE(restored->itemId, 7);
    QCOMPARE(restored->itemType, QString("value"));
    QCOMPARE(restored->pos(), QPointF(12.5, 34.25));
    QCOMPARE(restored->boundingRect().size(), QSizeF(150, 60));
    QCOMPARE(restored->zValue(), 2.5);
    QCOMPARE(restored->schemaVersion, 2);
    QCOMPARE(restored->commonStyle.value(QStringLiteral("fillColor")).toString(),
             QString("#ff8800"));
    QCOMPARE(restored->config.value(QStringLiteral("tagId")).toInt(), 42);
}

void UndoCommandsTest::removeItemCommand_redoAfterUndo_recreatesItem()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* item = scene.addItem(makeMeta());
    stack.push(new RemoveItemCommand(&scene, item, dashboardItemToJson(item)));

    stack.undo();
    QCOMPARE(scene.dashboardItems().size(), 1);

    stack.redo();
    QCOMPARE(scene.dashboardItems().size(), 0);
}

void UndoCommandsTest::deleteSelected_undo_restoresItems()
{
    DashboardScene scene;
    auto* a = scene.addItem(makeMeta(QStringLiteral("rect")));
    auto* b = scene.addItem(makeMeta(QStringLiteral("led")));
    a->setSelected(true);
    b->setSelected(true);

    scene.deleteSelected();
    QCOMPARE(scene.dashboardItems().size(), 0);
    QCOMPARE(scene.selectedItems().size(), 0);

    scene.undoStack()->undo(); // 批量删除是一个宏命令，一次撤销全部恢复
    QCOMPARE(scene.dashboardItems().size(), 2);
}

void UndoCommandsTest::moveCommand_mergesConsecutiveMoves()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* item = scene.addItem(makeMeta());
    item->setPos(0, 0);

    stack.push(new MoveCommand(item, QPointF(0, 0), QPointF(10, 0)));
    stack.push(new MoveCommand(item, QPointF(10, 0), QPointF(25, 5))); // 增量 >2px → 合并

    QCOMPARE(stack.count(), 1);
    stack.undo();
    QCOMPARE(item->pos(), QPointF(0, 0));
    stack.redo();
    QCOMPARE(item->pos(), QPointF(25, 5));
}

void UndoCommandsTest::moveCommand_microMove_doesNotMerge()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* item = scene.addItem(makeMeta());
    item->setPos(0, 0);

    stack.push(new MoveCommand(item, QPointF(0, 0), QPointF(10, 0)));
    stack.push(new MoveCommand(item, QPointF(10, 0), QPointF(10, 0.5))); // 0.5px 微动 → 不合并

    QCOMPARE(stack.count(), 2);
    stack.undo();
    QCOMPARE(item->pos(), QPointF(10, 0));
    stack.undo();
    QCOMPARE(item->pos(), QPointF(0, 0));
}

void UndoCommandsTest::multipleMoves_undo_returnsToInitialPosition()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* a = scene.addItem(makeMeta());
    auto* b = scene.addItem(makeMeta());
    auto* c = scene.addItem(makeMeta());

    stack.push(new MoveCommand(a, QPointF(0, 0), QPointF(100, 0)));
    stack.push(new MoveCommand(b, QPointF(0, 0), QPointF(0, 100)));
    stack.push(new MoveCommand(c, QPointF(0, 0), QPointF(50, 50)));

    stack.undo();
    stack.undo();
    stack.undo();

    QCOMPARE(a->pos(), QPointF(0, 0));
    QCOMPARE(b->pos(), QPointF(0, 0));
    QCOMPARE(c->pos(), QPointF(0, 0));
    QVERIFY(!stack.canUndo());
}

void UndoCommandsTest::resizeCommand_redoUndo_appliesRects()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* item = scene.addItem(makeMeta());
    item->setPos(10, 20);
    item->setSize(100, 50);

    const QRectF oldRect(item->pos(), item->boundingRect().size()); // (10,20,100,50)
    const QRectF newRect(30, 40, 200, 80);

    stack.push(new ResizeCommand(item, oldRect, newRect));
    QCOMPARE(item->pos(), QPointF(30, 40));
    QCOMPARE(item->boundingRect().size(), QSizeF(200, 80));

    stack.undo();
    QCOMPARE(item->pos(), QPointF(10, 20));
    QCOMPARE(item->boundingRect().size(), QSizeF(100, 50));
}

void UndoCommandsTest::resizeCommand_mergesConsecutiveResizes()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* item = scene.addItem(makeMeta());
    item->setPos(0, 0);
    item->setSize(100, 100);

    const QRectF r1(0, 0, 100, 100);
    const QRectF r2(0, 0, 150, 120);
    const QRectF r3(0, 0, 200, 150);

    stack.push(new ResizeCommand(item, r1, r2));
    stack.push(new ResizeCommand(item, r2, r3)); // 同一组件连续缩放 → 合并

    QCOMPARE(stack.count(), 1);
    stack.undo();
    QCOMPARE(item->pos(), QPointF(0, 0));
    QCOMPARE(item->boundingRect().size(), QSizeF(100, 100));
}

void UndoCommandsTest::propertyChangeCommand_undo_restoresOldValue()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* item = scene.addItem(makeMeta(QStringLiteral("value")));
    item->config = QJsonObject{{QStringLiteral("tagId"), 5}};
    item->commonStyle = QJsonObject{{QStringLiteral("fillColor"), QStringLiteral("#000000")}};

    // config 属性
    stack.push(new PropertyChangeCommand(item, QStringLiteral("tagId"),
                                         QJsonValue(5), QJsonValue(9)));
    QCOMPARE(item->config.value(QStringLiteral("tagId")).toInt(), 9);
    stack.undo();
    QCOMPARE(item->config.value(QStringLiteral("tagId")).toInt(), 5);

    // commonStyle 属性
    stack.push(new PropertyChangeCommand(item, QStringLiteral("fillColor"),
                                         QJsonValue(QStringLiteral("#000000")),
                                         QJsonValue(QStringLiteral("#ffffff"))));
    QCOMPARE(item->commonStyle.value(QStringLiteral("fillColor")).toString(),
             QString("#ffffff"));
    stack.undo();
    QCOMPARE(item->commonStyle.value(QStringLiteral("fillColor")).toString(),
             QString("#000000"));
}

void UndoCommandsTest::propertyChangeCommand_undefinedValue_removesProperty()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* item = scene.addItem(makeMeta());

    // key 不存在 → 目标容器为 commonStyle；Undefined 旧值表示"新增属性"，
    // undo 应删除该属性。
    stack.push(new PropertyChangeCommand(item, QStringLiteral("min"),
                                         QJsonValue(QJsonValue::Undefined),
                                         QJsonValue(0.0)));
    QVERIFY(item->commonStyle.contains(QStringLiteral("min")));
    QCOMPARE(item->commonStyle.value(QStringLiteral("min")).toDouble(), 0.0);

    stack.undo();
    QVERIFY(!item->commonStyle.contains(QStringLiteral("min")));
}

void UndoCommandsTest::propertyChangeCommand_commonStyle_roundTrips()
{
    DashboardScene scene;
    QUndoStack stack;
    auto* item = scene.addItem(makeMeta());
    item->commonStyle = QJsonObject{{QStringLiteral("borderWidth"), 1}};

    stack.push(new PropertyChangeCommand(item, QStringLiteral("borderWidth"),
                                         QJsonValue(1), QJsonValue(4)));
    QCOMPARE(item->commonStyle.value(QStringLiteral("borderWidth")).toInt(), 4);

    stack.undo();
    stack.redo();
    QCOMPARE(item->commonStyle.value(QStringLiteral("borderWidth")).toInt(), 4);
}

QTEST_MAIN(UndoCommandsTest)
#include "tst_UndoCommands.moc"
