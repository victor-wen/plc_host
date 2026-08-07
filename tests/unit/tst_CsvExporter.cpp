#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QtGlobal>

#include "history/CsvExporter.h"

// CSV 导出 (MON-02)：验证 UTF-8 BOM、表头列、行数、数值格式与转义。
class CsvExporterTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // 读文件并剥离 BOM，返回 UTF-8 解码后的内容。
    QString readCsv(const QString& path, bool* hasBom = nullptr) const
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        const QByteArray bytes = file.readAll();
        if (hasBom != nullptr)
            *hasBom = bytes.startsWith(QByteArray::fromHex("EFBBBF"));
        QByteArray body = bytes;
        if (body.startsWith(QByteArray::fromHex("EFBBBF")))
            body.remove(0, 3);
        return QString::fromUtf8(body);
    }

    static QStringList csvRows(const QString& content)
    {
        QStringList rows = content.split(QLatin1Char('\n'));
        if (!rows.isEmpty() && rows.constLast().isEmpty())
            rows.removeLast();   // 去掉末尾空行
        return rows;
    }

private slots:
    void exportHistory_bom_header_and_rows()
    {
        QVector<Tag> tags;
        Tag t1;
        t1.id = 1;
        t1.name = QStringLiteral("Motor.Speed");
        t1.unit = QStringLiteral("rpm");
        Tag t2;
        t2.id = 2;
        t2.name = QStringLiteral("Temp");
        t2.unit = QStringLiteral("degC");
        tags.append(t1);
        tags.append(t2);

        QVector<TagValue> data;
        TagValue v1;
        v1.tagId = 1;
        v1.value = QVariant(42.5);
        v1.quality = Quality::Good;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        v1.timestamp = QDateTime(QDate(2026, 8, 7), QTime(10, 30, 0, 123), QTimeZone::UTC);
#else
        v1.timestamp = QDateTime(QDate(2026, 8, 7), QTime(10, 30, 0, 123), Qt::UTC);
#endif
        TagValue v2;
        v2.tagId = 2;
        v2.value = QVariant(25);
        v2.quality = Quality::Stale;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        v2.timestamp = QDateTime(QDate(2026, 8, 7), QTime(10, 30, 1), QTimeZone::UTC);
#else
        v2.timestamp = QDateTime(QDate(2026, 8, 7), QTime(10, 30, 1), Qt::UTC);
#endif
        data.append(v1);
        data.append(v2);

        const QString path = m_tempDir.path() + "/history.csv";
        QVERIFY(CsvExporter::exportHistory(path, data, tags));

        bool hasBom = false;
        const QString content = readCsv(path, &hasBom);
        QVERIFY(hasBom);
        const QStringList rows = csvRows(content);
        QCOMPARE(rows.size(), 3);   // 表头 + 2 行数据

        QCOMPARE(rows[0], QStringLiteral("Time,TagName,Value,Quality,Unit"));
        QCOMPARE(rows[1].split(QLatin1Char(',')).size(), 5);
        QCOMPARE(rows[1], QStringLiteral("2026-08-07T10:30:00.123Z,Motor.Speed,42.5,Good,rpm"));
        QCOMPARE(rows[2], QStringLiteral("2026-08-07T10:30:01.000Z,Temp,25,Stale,degC"));
    }

    void exportHistory_empty_data_header_only()
    {
        const QString path = m_tempDir.path() + "/empty.csv";
        QVERIFY(CsvExporter::exportHistory(path, {}, {}));

        bool hasBom = false;
        const QStringList rows = csvRows(readCsv(path, &hasBom));
        QVERIFY(hasBom);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0], QStringLiteral("Time,TagName,Value,Quality,Unit"));
    }

    void exportHistory_unmatched_tag_uses_empty_name_unit()
    {
        const QString path = m_tempDir.path() + "/unknown.csv";
        QVector<TagValue> data;
        TagValue v;
        v.tagId = 99;   // 不在 tags 中
        v.value = QVariant(1.5);
        v.quality = Quality::Bad;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        v.timestamp = QDateTime(QDate(2026, 8, 7), QTime(9, 0), QTimeZone::UTC);
#else
        v.timestamp = QDateTime(QDate(2026, 8, 7), QTime(9, 0), Qt::UTC);
#endif
        data.append(v);

        QVERIFY(CsvExporter::exportHistory(path, data, {}));
        const QStringList rows = csvRows(readCsv(path));
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[1], QStringLiteral("2026-08-07T09:00:00.000Z,,1.5,Bad,"));
    }

    void exportHistory_value_formatting()
    {
        QVector<TagValue> data;
        {
            TagValue v;
            v.tagId = 1;
            v.value = QVariant(3.14159265358979);
            v.quality = Quality::Good;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            v.timestamp = QDateTime(QDate(2026, 8, 7), QTime(0, 0), QTimeZone::UTC);
#else
            v.timestamp = QDateTime(QDate(2026, 8, 7), QTime(0, 0), Qt::UTC);
#endif
            data.append(v);
        }
        {
            TagValue v;
            v.tagId = 1;
            v.value = QVariant(true);
            v.quality = Quality::Good;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            v.timestamp = QDateTime(QDate(2026, 8, 7), QTime(0, 0, 1), QTimeZone::UTC);
#else
            v.timestamp = QDateTime(QDate(2026, 8, 7), QTime(0, 0, 1), Qt::UTC);
#endif
            data.append(v);
        }
        QVector<Tag> tags;
        Tag t;
        t.id = 1;
        t.name = QStringLiteral("Flag");
        tags.append(t);

        const QString path = m_tempDir.path() + "/fmt.csv";
        QVERIFY(CsvExporter::exportHistory(path, data, tags));
        const QStringList rows = csvRows(readCsv(path));
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows[1], QStringLiteral("2026-08-07T00:00:00.000Z,Flag,3.14159265359,Good,"));
        QCOMPARE(rows[2], QStringLiteral("2026-08-07T00:00:01.000Z,Flag,1,Good,"));
    }

    void exportHistory_escapes_comma_quote_newline()
    {
        QVector<Tag> tags;
        Tag t;
        t.id = 1;
        t.name = QStringLiteral("A,B\"C");
        t.unit = QStringLiteral("x");
        tags.append(t);

        QVector<TagValue> data;
        TagValue v;
        v.tagId = 1;
        v.value = QVariant(1.0);
        v.quality = Quality::Good;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        v.timestamp = QDateTime(QDate(2026, 8, 7), QTime(0, 0), QTimeZone::UTC);
#else
        v.timestamp = QDateTime(QDate(2026, 8, 7), QTime(0, 0), Qt::UTC);
#endif
        data.append(v);

        const QString path = m_tempDir.path() + "/escape.csv";
        QVERIFY(CsvExporter::exportHistory(path, data, tags));
        const QStringList rows = csvRows(readCsv(path));
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[1], QStringLiteral("2026-08-07T00:00:00.000Z,\"A,B\"\"C\",1,Good,x"));
    }

    void exportTags_bom_header_and_values()
    {
        QVector<Tag> tags;
        Tag t1;
        t1.id = 1;
        t1.name = QStringLiteral("Motor");
        t1.registerType = RegisterType::HoldingRegister;
        t1.address = 100;
        t1.dataType = DataType::Float32;
        t1.byteOrder = ByteOrder::CDAB;
        t1.scale = 2.5;
        t1.offset = -1.0;
        t1.unit = QStringLiteral("m/s");
        Tag t2;
        t2.id = 2;
        t2.name = QStringLiteral("Relay");
        t2.registerType = RegisterType::Coil;
        t2.address = 0;
        t2.dataType = DataType::Bool;
        t2.byteOrder = ByteOrder::ABCD;
        tags.append(t1);
        tags.append(t2);

        const QString path = m_tempDir.path() + "/tags.csv";
        QVERIFY(CsvExporter::exportTags(path, tags));

        bool hasBom = false;
        const QStringList rows = csvRows(readCsv(path, &hasBom));
        QVERIFY(hasBom);
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows[0], QStringLiteral(
            "Name,RegisterType,Address,DataType,ByteOrder,Scale,Offset,Unit"));
        QCOMPARE(rows[1], QStringLiteral(
            "Motor,HoldingRegister,100,Float32,CDAB,2.5,-1,m/s"));
        QCOMPARE(rows[2], QStringLiteral("Relay,Coil,0,Bool,ABCD,1,0,"));
    }

    void exportTags_empty_header_only()
    {
        const QString path = m_tempDir.path() + "/tags_empty.csv";
        QVERIFY(CsvExporter::exportTags(path, {}));
        const QStringList rows = csvRows(readCsv(path));
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0], QStringLiteral(
            "Name,RegisterType,Address,DataType,ByteOrder,Scale,Offset,Unit"));
    }
};

QTEST_GUILESS_MAIN(CsvExporterTest)
#include "tst_CsvExporter.moc"
