#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QListWidget>
#include <QTest>
#include <QTemporaryDir>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>

#include "history/TrendService.h"
#include "ui/TrendWidget.h"

// 趋势页面 (MON-03)：验证订阅/取消订阅与 chart series 的联动、
// 实时点追加、点数窗口上限，以及导出数据按钮路径。
class TrendWidgetTest : public QObject {
    Q_OBJECT

private:
    static Tag makeTag(int id, const QString& name)
    {
        Tag t;
        t.id = id;
        t.name = name;
        t.unit = QStringLiteral("u");
        return t;
    }

    static QVector<Tag> twoTags()
    {
        QVector<Tag> tags;
        tags.append(makeTag(1, QStringLiteral("Alpha")));
        tags.append(makeTag(2, QStringLiteral("Beta")));
        return tags;
    }

private slots:
    void check_checked_adds_series_to_chart()
    {
        TrendWidget widget;
        widget.setTags(twoTags());
        auto* list = widget.findChild<QListWidget*>();
        QVERIFY(list != nullptr);
        QCOMPARE(list->count(), 2);
        QCOMPARE(widget.service()->chart()->series().size(), 0);

        // 勾选 → 订阅，series 出现在 chart 中
        list->item(0)->setCheckState(Qt::Checked);
        QCOMPARE(widget.service()->chart()->series().size(), 1);
        QLineSeries* s = widget.service()->seriesFor(1);
        QVERIFY(s != nullptr);
        QCOMPARE(s->name(), QStringLiteral("Alpha"));

        // 第二个 tag 订阅 → 两条 series
        list->item(1)->setCheckState(Qt::Checked);
        QCOMPARE(widget.service()->chart()->series().size(), 2);
        QVERIFY(widget.service()->seriesFor(2) != nullptr);
    }

    void uncheck_removes_series_from_chart()
    {
        TrendWidget widget;
        widget.setTags(twoTags());
        auto* list = widget.findChild<QListWidget*>();
        QVERIFY(list != nullptr);
        list->item(0)->setCheckState(Qt::Checked);
        list->item(1)->setCheckState(Qt::Checked);
        QCOMPARE(widget.service()->chart()->series().size(), 2);

        list->item(0)->setCheckState(Qt::Unchecked);
        QCOMPARE(widget.service()->chart()->series().size(), 1);
        QVERIFY(widget.service()->seriesFor(1) == nullptr);
        QVERIFY(widget.service()->seriesFor(2) != nullptr);

        list->item(1)->setCheckState(Qt::Unchecked);
        QCOMPARE(widget.service()->chart()->series().size(), 0);
    }

    void tagValueUpdate_appends_point_and_emits()
    {
        TrendWidget widget;
        widget.setTags(twoTags());
        auto* list = widget.findChild<QListWidget*>();
        QVERIFY(list != nullptr);
        list->item(0)->setCheckState(Qt::Checked);

        QLineSeries* series = widget.service()->seriesFor(1);
        QVERIFY(series != nullptr);
        QCOMPARE(series->count(), 0);

        int emitted = 0;
        int gotTag = -1;
        qint64 gotMs = 0;
        double gotValue = 0.0;
        connect(widget.service(), &TrendService::pointAdded, this,
            [&](int tagId, qint64 timestampMs, double value) {
                ++emitted;
                gotTag = tagId;
                gotMs = timestampMs;
                gotValue = value;
            });

        const QDateTime now = QDateTime(QDate(2026, 8, 7), QTime(10, 0, 0, 500), Qt::UTC);
        TagValue tv;
        tv.tagId = 1;
        tv.value = QVariant(42.5);
        tv.quality = Quality::Good;
        tv.timestamp = now;
        widget.onTagValueUpdated(tv);

        QCOMPARE(series->count(), 1);
        QCOMPARE(series->at(0).x(), static_cast<qreal>(now.toMSecsSinceEpoch()));
        QCOMPARE(series->at(0).y(), 42.5);
        QCOMPARE(emitted, 1);
        QCOMPARE(gotTag, 1);
        QCOMPARE(gotMs, now.toMSecsSinceEpoch());
        QCOMPARE(gotValue, 42.5);

        // 未订阅的 tag 被忽略
        TagValue other;
        other.tagId = 2;
        other.value = QVariant(1.0);
        other.timestamp = now;
        widget.onTagValueUpdated(other);
        QCOMPARE(series->count(), 1);
        QCOMPARE(emitted, 1);
    }

    void point_limit_prunes_old_points()
    {
        TrendWidget widget;
        widget.setTags(twoTags());
        auto* list = widget.findChild<QListWidget*>();
        auto* combo = widget.findChild<QComboBox*>();
        QVERIFY(list != nullptr);
        QVERIFY(combo != nullptr);

        combo->setCurrentIndex(0);   // 1m = 60s，上限 60*2=120 点
        list->item(0)->setCheckState(Qt::Checked);
        QLineSeries* series = widget.service()->seriesFor(1);
        QVERIFY(series != nullptr);

        const QDateTime t0(QDate(2026, 8, 7), QTime(0, 0), Qt::UTC);
        for (int i = 0; i < 150; ++i) {
            TagValue tv;
            tv.tagId = 1;
            tv.value = QVariant(double(i));
            tv.quality = Quality::Good;
            tv.timestamp = t0.addMSecs(i * 500);
            widget.onTagValueUpdated(tv);
        }
        QCOMPARE(series->count(), 120);
    }

    void exportDataToFile_writes_csv()
    {
        TrendWidget widget;
        widget.setTags(twoTags());
        auto* list = widget.findChild<QListWidget*>();
        QVERIFY(list != nullptr);
        list->item(0)->setCheckState(Qt::Checked);

        const QDateTime now = QDateTime(QDate(2026, 8, 7), QTime(10, 0), Qt::UTC);
        TagValue tv;
        tv.tagId = 1;
        tv.value = QVariant(7.25);
        tv.quality = Quality::Good;
        tv.timestamp = now;
        widget.onTagValueUpdated(tv);

        QTemporaryDir dir;
        const QString path = dir.path() + "/export.csv";
        QVERIFY(widget.exportDataToFile(path));
        QVERIFY(QFile::exists(path));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QVERIFY(bytes.startsWith(QByteArray::fromHex("EFBBBF")));
        const QString content = QString::fromUtf8(bytes.mid(3));
        QCOMPARE(content.split(QLatin1Char('\n'), Qt::SkipEmptyParts).size(), 2);   // 表头 + 1 行
        QVERIFY(content.contains(QStringLiteral("Alpha")));
        QVERIFY(content.contains(QStringLiteral("7.25")));
    }
};

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    TrendWidgetTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_TrendWidget.moc"
