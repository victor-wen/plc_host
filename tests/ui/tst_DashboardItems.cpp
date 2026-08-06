#include <QApplication>
#include <QEvent>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>

#include "dashboard/DashboardScene.h"
#include "dashboard/items/ImageItem.h"
#include "dashboard/items/LedItem.h"
#include "dashboard/items/RectItem.h"
#include "dashboard/items/SwitchItem.h"
#include "dashboard/items/TextItem.h"
#include "dashboard/items/ValueItem.h"

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

} // namespace

// 基础看板组件测试 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-05)：构造、属性、序列化往返、渲染冒烟与开关交互。
// 离屏渲染环境（见 main）。
class DashboardItemsTest : public QObject {
    Q_OBJECT
private slots:
    void factory_creates_six_components()
    {
        DashboardScene scene;
        scene.addItem(makeItem(QStringLiteral("text")));
        scene.addItem(makeItem(QStringLiteral("rect")));
        scene.addItem(makeItem(QStringLiteral("image")));
        scene.addItem(makeItem(QStringLiteral("value")));
        scene.addItem(makeItem(QStringLiteral("led")));
        scene.addItem(makeItem(QStringLiteral("switch")));
        // 未知类型仍降级为占位组件。
        scene.addItem(makeItem(QStringLiteral("gauge")));

        QCOMPARE(scene.dashboardItems().size(), 7);

        // 工厂创建的是具体组件而非占位组件。
        QVERIFY(dynamic_cast<TextItem*>(scene.dashboardItems()[0]) != nullptr);
        QVERIFY(dynamic_cast<RectItem*>(scene.dashboardItems()[1]) != nullptr);
        QVERIFY(dynamic_cast<ImageItem*>(scene.dashboardItems()[2]) != nullptr);
        QVERIFY(dynamic_cast<ValueItem*>(scene.dashboardItems()[3]) != nullptr);
        QVERIFY(dynamic_cast<LedItem*>(scene.dashboardItems()[4]) != nullptr);
        QVERIFY(dynamic_cast<SwitchItem*>(scene.dashboardItems()[5]) != nullptr);

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
    QApplication app(argc, argv);
    DashboardItemsTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_DashboardItems.moc"
