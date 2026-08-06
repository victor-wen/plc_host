#include <QApplication>
#include <QListWidget>
#include <QStackedWidget>
#include <QTest>

#include "app/MainWindow.h"
#include "runtime/AcquisitionEngine.h"
#include "ui/PlcConfigWidget.h"
#include "ui/TagEditorWidget.h"
#include "ui/TagMonitorWidget.h"

// UI 烟雾测试（CORE-09）：离屏渲染环境下验证主窗口构造、页面导航与三个
// 核心页面的模型行为。不发起任何 PLC 连接（不点击连接按钮）。
class UiSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void mainWindow_constructs_and_navigates()
    {
        MainWindow window;
        window.show();

        auto* nav = window.findChild<QListWidget*>(QStringLiteral("mainNav"));
        auto* stack = window.findChild<QStackedWidget*>(QStringLiteral("mainStack"));
        QVERIFY(nav != nullptr);
        QVERIFY(stack != nullptr);
        QCOMPARE(nav->count(), 3);
        QCOMPARE(stack->count(), 3);

        nav->setCurrentRow(1);
        QCOMPARE(stack->currentIndex(), 1);
        nav->setCurrentRow(2);
        QCOMPARE(stack->currentIndex(), 2);
        nav->setCurrentRow(0);
        QCOMPARE(stack->currentIndex(), 0);
    }

    void plcConfigWidget_fields_roundtrip()
    {
        PlcConfigWidget widget;
        PlcConfig cfg;
        cfg.host = QStringLiteral("10.0.0.5");
        cfg.port = 1502;
        cfg.unitId = 7;
        cfg.timeoutMs = 3000;
        cfg.retries = 5;
        cfg.pollIntervalMs = 1000;
        cfg.autoConnect = false;
        widget.setConfig(cfg);

        const PlcConfig out = widget.config();
        QCOMPARE(out.host, cfg.host);
        QCOMPARE(out.port, cfg.port);
        QCOMPARE(out.unitId, cfg.unitId);
        QCOMPARE(out.timeoutMs, cfg.timeoutMs);
        QCOMPARE(out.retries, cfg.retries);
        QCOMPARE(out.pollIntervalMs, cfg.pollIntervalMs);
        QCOMPARE(out.autoConnect, cfg.autoConnect);
    }

    void plcConfigWidget_defaults_apply()
    {
        PlcConfigWidget widget;   // 默认表单值来自 PlcConfig 默认值
        const PlcConfig out = widget.config();
        QCOMPARE(out.host, QStringLiteral("192.168.1.100"));
        QCOMPARE(out.port, 502);
        QCOMPARE(out.unitId, 1);
        QCOMPARE(out.timeoutMs, 1000);
        QCOMPARE(out.retries, 2);
        QCOMPARE(out.pollIntervalMs, 500);
        QCOMPARE(out.autoConnect, true);
    }

    void tagEditorModel_crud_and_edit()
    {
        TagEditorWidget editor;
        QCOMPARE(editor.model()->rowCount(), 0);

        const int row = editor.model()->addTag();
        QCOMPARE(editor.model()->rowCount(), 1);
        QCOMPARE(editor.model()->tagAt(row).id, 1);

        // 名称编辑
        QVERIFY(editor.model()->setData(
            editor.model()->index(row, static_cast<int>(TagColumn::Name)),
            QStringLiteral("Motor.Speed")));
        QCOMPARE(editor.model()->tagAt(row).name, QStringLiteral("Motor.Speed"));

        // 空名称拒绝
        QVERIFY(!editor.model()->setData(
            editor.model()->index(row, static_cast<int>(TagColumn::Name)), QStringLiteral("  ")));

        // 地址与数据类型编辑
        QVERIFY(editor.model()->setData(
            editor.model()->index(row, static_cast<int>(TagColumn::Address)), 100));
        QCOMPARE(editor.model()->tagAt(row).address, 100);
        QVERIFY(!editor.model()->setData(
            editor.model()->index(row, static_cast<int>(TagColumn::Address)), -1));

        QVERIFY(editor.model()->setData(
            editor.model()->index(row, static_cast<int>(TagColumn::DataType)),
            static_cast<int>(DataType::Float32)));
        QCOMPARE(editor.model()->tagAt(row).dataType, DataType::Float32);

        // scale=0 拒绝（防除零）
        QVERIFY(!editor.model()->setData(
            editor.model()->index(row, static_cast<int>(TagColumn::Scale)), 0.0));

        // 追加（CSV 导入路径）
        Tag imported;
        imported.name = QStringLiteral("Temp");
        imported.address = 200;
        imported.dataType = DataType::Int16;
        const int row2 = editor.model()->appendTag(imported);
        QCOMPARE(editor.model()->rowCount(), 2);
        QVERIFY(editor.model()->tagAt(row2).id != editor.model()->tagAt(row).id);

        // 删除
        QVERIFY(editor.model()->removeTag(row));
        QCOMPARE(editor.model()->rowCount(), 1);
    }

    void tagEditorModel_move_up_down()
    {
        TagEditorModel model;
        Tag a;
        a.id = 1;
        a.name = QStringLiteral("A");
        Tag b;
        b.id = 2;
        b.name = QStringLiteral("B");
        model.setTags({a, b});

        QVERIFY(model.moveTag(1, -1));   // B 上移
        QCOMPARE(model.tagAt(0).name, QStringLiteral("B"));
        QCOMPARE(model.tagAt(1).name, QStringLiteral("A"));
        QVERIFY(!model.moveTag(0, -1));  // 越界拒绝
        QVERIFY(model.moveTag(0, 1));    // B 下移还原
        QCOMPARE(model.tagAt(0).name, QStringLiteral("A"));
    }

    void tagMonitorModel_values_colors_and_filter()
    {
        TagMonitorModel model;
        QVector<Tag> tags;
        Tag t1;
        t1.id = 1;
        t1.name = QStringLiteral("Alpha");
        t1.unit = QStringLiteral("rpm");
        Tag t2;
        t2.id = 2;
        t2.name = QStringLiteral("Beta");
        tags.append(t1);
        tags.append(t2);
        model.setTags(tags);
        QCOMPARE(model.rowCount(), 2);

        QHash<int, TagValue> values;
        TagValue good;
        good.tagId = 1;
        good.value = 42;
        good.quality = Quality::Good;
        values.insert(1, good);
        model.updateValues(values);

        QCOMPARE(model.index(0, 1).data().toString(), QStringLiteral("42"));
        QCOMPARE(model.index(0, 3).data().toString(), QStringLiteral("Good"));
        QCOMPARE(model.index(0, 5).data().toString(), QStringLiteral("rpm"));
        // 未更新 tag 显示占位与 Disconnected
        QCOMPARE(model.index(1, 1).data().toString(), QStringLiteral("--"));
        QCOMPARE(model.index(1, 3).data().toString(), QStringLiteral("Disconnected"));

        // 质量列颜色
        const QColor color = model.index(0, 3).data(Qt::BackgroundRole).value<QColor>();
        QCOMPARE(color.name(), TagMonitorModel::qualityColor(Quality::Good).name());

        // 搜索过滤（大小写不敏感）
        model.setFilter(QStringLiteral("beta"));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.index(0, 0).data().toString(), QStringLiteral("Beta"));
        model.setFilter(QStringLiteral("zzz"));
        QCOMPARE(model.rowCount(), 0);
        model.setFilter(QString());
        QCOMPARE(model.rowCount(), 2);
    }
};

int main(int argc, char* argv[])
{
    // 无显示环境下使用离屏平台运行 UI 测试。
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    UiSmokeTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_UiSmoke.moc"
