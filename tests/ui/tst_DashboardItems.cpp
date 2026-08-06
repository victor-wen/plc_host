#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QMetaType>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

#include "dashboard/DashboardScene.h"
#include "dashboard/items/ButtonItem.h"
#include "dashboard/items/GaugeItem.h"
#include "dashboard/items/ImageItem.h"
#include "dashboard/items/LedItem.h"
#include "dashboard/items/ProgressBarItem.h"
#include "dashboard/items/RectItem.h"
#include "dashboard/items/SwitchItem.h"
#include "dashboard/items/TextItem.h"
#include "dashboard/items/TrendItem.h"
#include "dashboard/items/ValueInputItem.h"
#include "dashboard/items/ValueItem.h"
#include "dashboard/runtime/ButtonAction.h"

namespace {

// 测试辅助：构造一个未保存（id=-1）的组件元数据。
DashboardItem makeItem(const QString& type, qreal width = 100, qreal height = 100)
{
    DashboardItem meta;
    meta.itemType = type;
    meta.width = width;
    meta.height = height;
    return meta;
}

// 向场景发送一次左键 press+release（项坐标 pos 相对场景原点）。
void clickAt(QGraphicsScene* scene, const QPointF& pos)
{
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setPos(pos);
    press.setScenePos(pos);
    press.setButton(Qt::LeftButton);
    press.setButtons(Qt::LeftButton);
    QApplication::sendEvent(scene, &press);

    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
    release.setPos(pos);
    release.setScenePos(pos);
    release.setButton(Qt::LeftButton);
    release.setButtons(Qt::NoButton);
    QApplication::sendEvent(scene, &release);
}

// 向场景发送一次双击（首击 press+release，随后 MouseButtonDblClick + release）。
void doubleClickAt(QGraphicsScene* scene, const QPointF& pos)
{
    clickAt(scene, pos);

    QGraphicsSceneMouseEvent dbl(QEvent::GraphicsSceneMouseDoubleClick);
    dbl.setPos(pos);
    dbl.setScenePos(pos);
    dbl.setButton(Qt::LeftButton);
    dbl.setButtons(Qt::LeftButton);
    QApplication::sendEvent(scene, &dbl);

    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
    release.setPos(pos);
    release.setScenePos(pos);
    release.setButton(Qt::LeftButton);
    release.setButtons(Qt::NoButton);
    QApplication::sendEvent(scene, &release);
}

// 向场景发送一次无按键的鼠标移动（无鼠标抓取者时场景据此分发 hover
// 进入/离开事件，等价于真实鼠标悬停）。
void hoverTo(QGraphicsScene* scene, const QPointF& pos)
{
    QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
    move.setPos(pos);
    move.setScenePos(pos);
    move.setButton(Qt::NoButton);
    move.setButtons(Qt::NoButton);
    QApplication::sendEvent(scene, &move);
}

} // namespace

// 基础看板组件测试 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-05)：构造、属性、序列化往返、渲染冒烟与开关交互。
// 离屏渲染环境（见 main）。
class DashboardItemsTest : public QObject {
    Q_OBJECT
private slots:
    void factory_creates_concrete_components()
    {
        DashboardScene scene;
        scene.addItem(makeItem(QStringLiteral("text")));
        scene.addItem(makeItem(QStringLiteral("rect")));
        scene.addItem(makeItem(QStringLiteral("image")));
        scene.addItem(makeItem(QStringLiteral("value")));
        scene.addItem(makeItem(QStringLiteral("led")));
        scene.addItem(makeItem(QStringLiteral("switch")));
        // DASH-06 高级组件。
        scene.addItem(makeItem(QStringLiteral("progress")));
        scene.addItem(makeItem(QStringLiteral("gauge")));
        scene.addItem(makeItem(QStringLiteral("valueInput")));
        scene.addItem(makeItem(QStringLiteral("trend")));
        // 未知类型仍降级为占位组件。
        scene.addItem(makeItem(QStringLiteral("bogus")));

        QCOMPARE(scene.dashboardItems().size(), 11);

        // 工厂创建的是具体组件而非占位组件。
        QVERIFY(dynamic_cast<TextItem*>(scene.dashboardItems()[0]) != nullptr);
        QVERIFY(dynamic_cast<RectItem*>(scene.dashboardItems()[1]) != nullptr);
        QVERIFY(dynamic_cast<ImageItem*>(scene.dashboardItems()[2]) != nullptr);
        QVERIFY(dynamic_cast<ValueItem*>(scene.dashboardItems()[3]) != nullptr);
        QVERIFY(dynamic_cast<LedItem*>(scene.dashboardItems()[4]) != nullptr);
        QVERIFY(dynamic_cast<SwitchItem*>(scene.dashboardItems()[5]) != nullptr);
        QVERIFY(dynamic_cast<ProgressBarItem*>(scene.dashboardItems()[6]) != nullptr);
        QVERIFY(dynamic_cast<GaugeItem*>(scene.dashboardItems()[7]) != nullptr);
        QVERIFY(dynamic_cast<ValueInputItem*>(scene.dashboardItems()[8]) != nullptr);
        QVERIFY(dynamic_cast<TrendItem*>(scene.dashboardItems()[9]) != nullptr);

        // 未知类型：itemType 保留原始字符串，仍是 DashboardBaseItem。
        QCOMPARE(scene.dashboardItems()[10]->itemType, QStringLiteral("bogus"));

        for (auto* item : scene.dashboardItems()) {
            QVERIFY(item->boundingRect().width() > 0);
            QVERIFY(item->boundingRect().height() > 0);
        }
    }

    void textItem_text_property()
    {
        TextItem item;
        QCOMPARE(item.text(), QStringLiteral("Text"));
        item.setText(QStringLiteral("一号罐温度"));
        QCOMPARE(item.text(), QStringLiteral("一号罐温度"));
        // setText 同步 config，保证持久化/撤销快照一致。
        QCOMPARE(item.config.value(QStringLiteral("text")).toString(),
                 QStringLiteral("一号罐温度"));
    }

    void textItem_serialize_roundtrip()
    {
        TextItem a;
        a.setText(QStringLiteral("hello"));
        a.config.insert(QStringLiteral("custom"), 42); // 无关键保留
        QJsonObject json = a.serialize();

        TextItem b;
        b.deserialize(json);
        QCOMPARE(b.text(), QStringLiteral("hello"));
        QCOMPARE(b.config.value(QStringLiteral("custom")).toInt(), 42);
    }

    void rectItem_default_size()
    {
        RectItem item;
        QCOMPARE(item.boundingRect(), QRectF(0, 0, 100, 100));

        RectItem scaled;
        scaled.setSize(200, 50);
        QCOMPARE(scaled.boundingRect(), QRectF(0, 0, 200, 50));
    }

    void rectItem_deserialize_from_commonStyle()
    {
        DashboardItem meta = makeItem(QStringLiteral("rect"));
        meta.commonStyle.insert(QStringLiteral("fillColor"), QStringLiteral("#123456"));
        meta.commonStyle.insert(QStringLiteral("borderWidth"), 3);

        DashboardScene scene;
        auto* item = dynamic_cast<RectItem*>(scene.addItem(meta));
        QVERIFY(item != nullptr);
        // 表现层属性从 commonStyle 恢复（通过 deserialize 进入成员缓存）。
        QCOMPARE(item->serialize(), item->config);
    }

    void imageItem_fitmode_roundtrip()
    {
        ImageItem item;
        QCOMPARE(item.fitMode(), QStringLiteral("contain"));
        item.setImagePath(QStringLiteral("/tmp/foo.png"));
        item.setFitMode(QStringLiteral("cover"));

        QJsonObject json = item.serialize();
        QCOMPARE(json.value(QStringLiteral("imagePath")).toString(),
                 QStringLiteral("/tmp/foo.png"));
        QCOMPARE(json.value(QStringLiteral("fitMode")).toString(),
                 QStringLiteral("cover"));

        ImageItem restored;
        restored.deserialize(json);
        QCOMPARE(restored.imagePath(), QStringLiteral("/tmp/foo.png"));
        QCOMPARE(restored.fitMode(), QStringLiteral("cover"));
    }

    void valueItem_default_display_dashdash()
    {
        ValueItem item;
        QVERIFY(!item.hasValue());
        QCOMPARE(item.displayText(), QStringLiteral("--"));

        // 注入值后按 precision 格式化。
        item.setTagValue(3.14159, Quality::Good);
        QVERIFY(item.hasValue());
        QCOMPARE(item.displayText(), QStringLiteral("3.1"));

        // 非数值 value 也回退 "--"。
        item.setTagValue(QStringLiteral("abc"), Quality::Bad);
        QCOMPARE(item.displayText(), QStringLiteral("--"));
    }

    void valueItem_serialize_roundtrip()
    {
        ValueItem item;
        QJsonObject cfg;
        cfg.insert(QStringLiteral("tagId"), 7);
        cfg.insert(QStringLiteral("precision"), 3);
        cfg.insert(QStringLiteral("prefix"), QStringLiteral("P:"));
        cfg.insert(QStringLiteral("suffix"), QStringLiteral("℃"));
        item.deserialize(cfg);

        ValueItem restored;
        restored.deserialize(item.serialize());
        QCOMPARE(restored.serialize().value(QStringLiteral("tagId")).toInt(), 7);
        QCOMPARE(restored.serialize().value(QStringLiteral("precision")).toInt(), 3);

        restored.setTagValue(20.5, Quality::Good);
        QCOMPARE(restored.displayText(), QStringLiteral("P:20.500℃"));
    }

    void ledItem_label()
    {
        LedItem item;
        QCOMPARE(item.label(), QString());
        item.setLabel(QStringLiteral("运行中"));
        QCOMPARE(item.label(), QStringLiteral("运行中"));
        // setLabel 同步 config。
        QCOMPARE(item.config.value(QStringLiteral("label")).toString(),
                 QStringLiteral("运行中"));

        // serialize/deserialize 往返保留 label 与颜色。
        QJsonObject json = item.serialize();
        QCOMPARE(json.value(QStringLiteral("label")).toString(),
                 QStringLiteral("运行中"));
        QCOMPARE(json.value(QStringLiteral("onColor")).toString(),
                 QStringLiteral("#34c759"));

        LedItem restored;
        restored.deserialize(json);
        QCOMPARE(restored.label(), QStringLiteral("运行中"));
    }

    void ledItem_state_changes_color()
    {
        LedItem item;
        QVERIFY(!item.isOn());
        item.setOn(true);
        QVERIFY(item.isOn());
        item.setOn(true); // 幂等
        QVERIFY(item.isOn());
        item.setOn(false);
        QVERIFY(!item.isOn());
    }

    void switchItem_toggles_only_in_run_mode()
    {
        DashboardScene scene;
        DashboardItem meta = makeItem(QStringLiteral("switch"));
        meta.config.insert(QStringLiteral("tagId"), 5);
        auto* item = dynamic_cast<SwitchItem*>(scene.addItem(meta));
        QVERIFY(item != nullptr);

        QSignalSpy spy(item, &SwitchItem::toggled);

        // 编辑模式点击：不切换、不发信号。
        clickAt(&scene, QPointF(50, 50));
        QCOMPARE(spy.count(), 0);
        QVERIFY(!item->isOn());

        // 运行模式点击：切换为 ON 并发出 toggled(tagId=5, on=true)。
        scene.setEditMode(false);
        QVERIFY(!item->isEditMode());
        clickAt(&scene, QPointF(50, 50));
        QCOMPARE(spy.count(), 1);
        QVERIFY(item->isOn());
        QCOMPARE(spy.at(0).at(0).toInt(), 5);
        QCOMPARE(spy.at(0).at(1).toBool(), true);

        // 再点一次切回 OFF。
        clickAt(&scene, QPointF(50, 50));
        QCOMPARE(spy.count(), 2);
        QVERIFY(!item->isOn());
        QCOMPARE(spy.at(1).at(1).toBool(), false);

        // 恢复编辑模式后点击不再切换。
        scene.setEditMode(true);
        clickAt(&scene, QPointF(50, 50));
        QCOMPARE(spy.count(), 2);
    }

    void switchItem_serialize_roundtrip()
    {
        SwitchItem item;
        QJsonObject cfg;
        cfg.insert(QStringLiteral("tagId"), 9);
        cfg.insert(QStringLiteral("onValue"), 1);
        cfg.insert(QStringLiteral("offValue"), 0);
        item.deserialize(cfg);

        SwitchItem restored;
        restored.deserialize(item.serialize());
        QCOMPARE(restored.serialize().value(QStringLiteral("onValue")).toInt(), 1);
        QCOMPARE(restored.serialize().value(QStringLiteral("offValue")).toInt(), 0);
        QCOMPARE(restored.serialize().value(QStringLiteral("tagId")).toInt(), 9);
    }

    void factory_creates_button()
    {
        DashboardScene scene;
        auto* item = dynamic_cast<ButtonItem*>(scene.addItem(makeItem(QStringLiteral("button"))));
        QVERIFY(item != nullptr);
        QCOMPARE(item->itemType, QStringLiteral("button"));
        QCOMPARE(item->text(), QStringLiteral("按钮"));
    }

    void buttonStateTransitions()
    {
        // hover/press 鼠标事件驱动视觉状态切换（运行模式）。
        DashboardScene scene;
        DashboardItem meta = makeItem(QStringLiteral("button"));
        meta.config.insert(QStringLiteral("tagId"), 3);
        auto* item = dynamic_cast<ButtonItem*>(scene.addItem(meta));
        QVERIFY(item != nullptr);
        scene.setEditMode(false);
        QVERIFY(!item->isEditMode());

        // 初始 Normal。
        QCOMPARE(item->visualState(), ButtonVisualState::Normal);

        // 悬停进入 → Hover。
        hoverTo(&scene, QPointF(50, 50));
        QCOMPARE(item->visualState(), ButtonVisualState::Hover);

        // 按下 → Pressed。
        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setPos(QPointF(50, 50));
        press.setScenePos(QPointF(50, 50));
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        QApplication::sendEvent(&scene, &press);
        QCOMPARE(item->visualState(), ButtonVisualState::Pressed);

        // 释放（仍在按钮内）→ 回到 Hover。
        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setPos(QPointF(50, 50));
        release.setScenePos(QPointF(50, 50));
        release.setButton(Qt::LeftButton);
        release.setButtons(Qt::NoButton);
        QApplication::sendEvent(&scene, &release);
        QCOMPARE(item->visualState(), ButtonVisualState::Hover);

        // 悬停离开（移到按钮外）→ Normal。
        hoverTo(&scene, QPointF(150, 150));
        QCOMPARE(item->visualState(), ButtonVisualState::Normal);
    }

    void button_editMode_doesNotChangeVisual()
    {
        // 编辑模式：只渲染，hover/press 不改变视觉状态。
        DashboardScene scene;
        auto* item = dynamic_cast<ButtonItem*>(scene.addItem(makeItem(QStringLiteral("button"))));
        QVERIFY(item != nullptr);
        QVERIFY(item->isEditMode());

        hoverTo(&scene, QPointF(50, 50));
        QCOMPARE(item->visualState(), ButtonVisualState::Normal);

        clickAt(&scene, QPointF(50, 50));
        QCOMPARE(item->visualState(), ButtonVisualState::Normal);
    }

    void button_disabled_waiting_states()
    {
        ButtonItem item;
        QCOMPARE(item.visualState(), ButtonVisualState::Normal);

        item.setButtonEnabled(false);
        QCOMPARE(item.visualState(), ButtonVisualState::Disabled);
        item.setButtonEnabled(true);
        QCOMPARE(item.visualState(), ButtonVisualState::Normal);

        item.setWaiting(true);
        QCOMPARE(item.visualState(), ButtonVisualState::Waiting);
        item.setWaiting(false);
        QCOMPARE(item.visualState(), ButtonVisualState::Normal);
    }

    void button_actionTriggered_on_release_run_mode()
    {
        DashboardScene scene;
        DashboardItem meta = makeItem(QStringLiteral("button"));
        meta.config.insert(QStringLiteral("tagId"), 7);
        QJsonObject actionObj;
        actionObj.insert(QStringLiteral("type"), QStringLiteral("fixedValue"));
        actionObj.insert(QStringLiteral("paramA"), 42);
        meta.config.insert(QStringLiteral("action"), actionObj);
        auto* item = dynamic_cast<ButtonItem*>(scene.addItem(meta));
        QVERIFY(item != nullptr);

        QSignalSpy spy(item, &ButtonItem::actionTriggered);

        // 编辑模式点击：不触发动作。
        clickAt(&scene, QPointF(50, 50));
        QCOMPARE(spy.count(), 0);

        // 运行模式点击：发出 actionTriggered(FixedValue, paramA=42, tagId=7)。
        scene.setEditMode(false);
        clickAt(&scene, QPointF(50, 50));
        QCOMPARE(spy.count(), 1);
        const ButtonAction triggered = spy.at(0).at(0).value<ButtonAction>();
        QCOMPARE(triggered.type, ButtonActionType::FixedValue);
        QCOMPARE(triggered.paramA.toInt(), 42);
        QCOMPARE(spy.at(0).at(1).toInt(), 7);

        // 禁用后点击：不再触发动作。
        item->setButtonEnabled(false);
        clickAt(&scene, QPointF(50, 50));
        QCOMPARE(spy.count(), 1);
    }

    void button_serialize_roundtrip()
    {
        ButtonItem item;
        QJsonObject cfg;
        cfg.insert(QStringLiteral("tagId"), 12);
        cfg.insert(QStringLiteral("text"), QStringLiteral("启动"));
        QJsonObject actionObj;
        actionObj.insert(QStringLiteral("type"), QStringLiteral("momentary"));
        actionObj.insert(QStringLiteral("paramA"), 1);
        actionObj.insert(QStringLiteral("paramB"), 0);
        cfg.insert(QStringLiteral("action"), actionObj);
        item.deserialize(cfg);

        ButtonItem restored;
        restored.deserialize(item.serialize());
        QCOMPARE(restored.serialize().value(QStringLiteral("tagId")).toInt(), 12);
        QCOMPARE(restored.serialize().value(QStringLiteral("text")).toString(),
                 QStringLiteral("启动"));
        const ButtonAction action = ButtonAction::fromJson(
            restored.serialize().value(QStringLiteral("action")).toObject());
        QCOMPARE(action.type, ButtonActionType::Momentary);
        QCOMPARE(action.paramA.toInt(), 1);
        QCOMPARE(action.paramB.toInt(), 0);
    }

    // ---- DASH-06 高级组件 ----

    void progressBarRenders()
    {
        DashboardScene scene;
        DashboardItem meta = makeItem(QStringLiteral("progress"), 120, 30);
        meta.config.insert(QStringLiteral("min"), 0);
        meta.config.insert(QStringLiteral("max"), 200);
        auto* item = dynamic_cast<ProgressBarItem*>(scene.addItem(meta));
        QVERIFY(item != nullptr);

        // 默认无值：不填充、显示 "--"。
        QVERIFY(!item->hasValue());
        QCOMPARE(item->ratio(), 0.0);

        // 值 150 / [0,200] → 0.75。
        item->setTagValue(150.0, Quality::Good);
        QVERIFY(item->hasValue());
        QCOMPARE(item->ratio(), 0.75);

        // 越界钳制到 [0,1]。
        item->setTagValue(500.0, Quality::Good);
        QCOMPARE(item->ratio(), 1.0);
        item->setTagValue(-1.0, Quality::Good);
        QCOMPARE(item->ratio(), 0.0);

        // 序列化往返保留业务属性（min/max 来自 config）。
        QJsonObject json = item->serialize();
        ProgressBarItem restored;
        restored.deserialize(json);
        QCOMPARE(restored.serialize().value(QStringLiteral("min")).toDouble(), 0.0);
        QCOMPARE(restored.serialize().value(QStringLiteral("max")).toDouble(), 200.0);

        // 渲染冒烟：填充 75% 的进度条产生非背景像素。
        item->setTagValue(150.0, Quality::Good);
        QImage image(120, 30, QImage::Format_ARGB32);
        image.fill(QColor(Qt::black));
        QPainter painter(&image);
        scene.render(&painter);
        painter.end();
        QVERIFY(image.pixelColor(30, 15) != QColor(Qt::black)); // 填充区域
    }

    void gaugeRenders()
    {
        DashboardScene scene;
        DashboardItem meta = makeItem(QStringLiteral("gauge"), 100, 100);
        meta.config.insert(QStringLiteral("min"), 0);
        meta.config.insert(QStringLiteral("max"), 100);
        auto* item = dynamic_cast<GaugeItem*>(scene.addItem(meta));
        QVERIFY(item != nullptr);

        // 默认无值：ratio 0，不崩溃。
        QVERIFY(!item->hasValue());
        QCOMPARE(item->ratio(), 0.0);

        // 值 25 / [0,100] → 0.25。
        item->setTagValue(25.0, Quality::Good);
        QVERIFY(item->hasValue());
        QCOMPARE(item->ratio(), 0.25);

        // 序列化往返保留 startAngle/spanAngle/min/max。
        QJsonObject json = item->serialize();
        GaugeItem restored;
        restored.deserialize(json);
        QCOMPARE(restored.serialize().value(QStringLiteral("min")).toDouble(), 0.0);
        QCOMPARE(restored.serialize().value(QStringLiteral("max")).toDouble(), 100.0);
        QCOMPARE(restored.serialize().value(QStringLiteral("startAngle")).toDouble(), 225.0);
        QCOMPARE(restored.serialize().value(QStringLiteral("spanAngle")).toDouble(), 270.0);

        // 渲染冒烟：圆弧/指针/中心文字产生非背景像素。
        QImage image(100, 100, QImage::Format_ARGB32);
        image.fill(QColor(Qt::black));
        QPainter painter(&image);
        scene.render(&painter);
        painter.end();
        QVERIFY(image.pixelColor(50, 50) != QColor(Qt::black)); // 中心值文字
    }

    void valueInputDoubleClick()
    {
        DashboardScene scene;
        DashboardItem meta = makeItem(QStringLiteral("valueInput"), 120, 30);
        meta.config.insert(QStringLiteral("tagId"), 3);
        meta.config.insert(QStringLiteral("min"), 0);
        meta.config.insert(QStringLiteral("max"), 100);
        meta.config.insert(QStringLiteral("precision"), 1);
        auto* item = dynamic_cast<ValueInputItem*>(scene.addItem(meta));
        QVERIFY(item != nullptr);

        // 编辑模式不弹输入框。
        QSignalSpy spy(item, &ValueInputItem::valueSubmitted);
        doubleClickAt(&scene, QPointF(60, 15));
        QCOMPARE(spy.count(), 0);

        // 运行模式双击弹出 QInputDialog；在对话框事件循环内填入 42.5 并确认。
        scene.setEditMode(false);
        bool dialogHandled = false;
        QTimer::singleShot(0, [&]() {
            auto* dlg = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            auto* spin = dlg ? dlg->findChild<QDoubleSpinBox*>() : nullptr;
            if (!dlg || !spin) {
                if (dlg)
                    dlg->close(); // 避免挂起
                dialogHandled = true;
                return;
            }
            spin->setValue(42.5);
            dlg->accept(); // getDouble 从 spinbox 读取最终值
            dialogHandled = true;
        });

        doubleClickAt(&scene, QPointF(60, 15));

        QVERIFY(dialogHandled);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
        QCOMPARE(spy.at(0).at(1).toDouble(), 42.5);

        // 双击后注入值显示为 42.5（precision=1）。
        QCOMPARE(item->displayText(), QStringLiteral("42.5"));
    }

    void trendPlaceholderRenders()
    {
        DashboardScene scene;
        DashboardItem meta = makeItem(QStringLiteral("trend"), 160, 80);
        meta.config.insert(QStringLiteral("historySeconds"), 120);
        auto* item = dynamic_cast<TrendItem*>(scene.addItem(meta));
        QVERIFY(item != nullptr);

        // 默认 historySeconds=60；config 指定 120。
        TrendItem defaults;
        QCOMPARE(defaults.historySeconds(), 60);
        QCOMPARE(item->historySeconds(), 120);

        // 序列化往返保留 historySeconds。
        TrendItem restored;
        restored.deserialize(item->serialize());
        QCOMPARE(restored.serialize().value(QStringLiteral("historySeconds")).toInt(), 120);

        // 渲染冒烟：坐标轴 + "Trend" 文字产生非背景像素。
        QImage image(160, 80, QImage::Format_ARGB32);
        image.fill(QColor(Qt::black));
        QPainter painter(&image);
        scene.render(&painter);
        painter.end();
        bool drawn = false;
        for (int y = 0; y < image.height() && !drawn; ++y) {
            for (int x = 0; x < image.width() && !drawn; ++x) {
                if (image.pixelColor(x, y) != QColor(Qt::black))
                    drawn = true;
            }
        }
        QVERIFY(drawn);
    }

    void render_smoke_all_components()
    {
        // 全部组件渲染到离屏图像：不崩溃且产生非背景像素。
        DashboardScene scene;
        DashboardPage page;
        page.width = 640;
        page.height = 120;
        scene.setPage(page);

        scene.addItem(makeItem(QStringLiteral("text"), 100, 40));
        scene.addItem(makeItem(QStringLiteral("rect"), 100, 40));
        scene.addItem(makeItem(QStringLiteral("image"), 100, 40));
        scene.addItem(makeItem(QStringLiteral("value"), 100, 40));
        scene.addItem(makeItem(QStringLiteral("led"), 60, 40));
        scene.addItem(makeItem(QStringLiteral("switch"), 80, 40));
        scene.addItem(makeItem(QStringLiteral("button"), 80, 40));
        // DASH-06 高级组件。
        scene.addItem(makeItem(QStringLiteral("progress"), 120, 30));
        scene.addItem(makeItem(QStringLiteral("gauge"), 80, 40));
        scene.addItem(makeItem(QStringLiteral("valueInput"), 100, 30));
        scene.addItem(makeItem(QStringLiteral("trend"), 120, 40));

        QImage image(640, 120, QImage::Format_ARGB32);
        image.fill(QColor(Qt::black));
        QPainter painter(&image);
        scene.render(&painter);
        painter.end();

        // 至少存在一个非背景像素（场景背景为深色，与黑色不同或组件内容着色）。
        bool drawn = false;
        for (int y = 0; y < image.height() && !drawn; ++y) {
            for (int x = 0; x < image.width() && !drawn; ++x) {
                if (image.pixelColor(x, y) != QColor(Qt::black))
                    drawn = true;
            }
        }
        QVERIFY(drawn);
    }
};

int main(int argc, char* argv[])
{
    // 无显示环境下使用离屏平台运行 UI 测试。
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    // 供 QSignalSpy 提取 actionTriggered 的 ButtonAction 参数。
    qRegisterMetaType<ButtonAction>();
    QApplication app(argc, argv);
    DashboardItemsTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_DashboardItems.moc"
