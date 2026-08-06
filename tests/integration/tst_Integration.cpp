#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QModbusTcpServer>
#include <QModbusDataUnit>

#include "modbus/QtModbusClient.h"
#include "runtime/TagCache.h"
#include "runtime/WriteQueue.h"
#include "runtime/AcquisitionEngine.h"
#include "domain/Tag.h"
#include "domain/TagValue.h"
#include "storage/DatabaseMigrator.h"
#include "history/HistoryService.h"
#include "alarm/AlarmEngine.h"
#include "alarm/AlarmRule.h"

class IntegrationTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    QVector<Tag> createTestTags() {
        Tag t1; t1.id = 1; t1.name = "Test16"; t1.address = 0;
        t1.registerType = RegisterType::HoldingRegister;
        t1.dataType = DataType::UInt16; t1.byteOrder = ByteOrder::ABCD;
        Tag t2; t2.id = 2; t2.name = "Test32"; t2.address = 1;
        t2.registerType = RegisterType::HoldingRegister;
        t2.dataType = DataType::Int32; t2.byteOrder = ByteOrder::ABCD;
        return {t1, t2};
    }

private slots:
    void writeQueue_full_lifecycle() {
        WriteQueue queue;
        QVERIFY(queue.isEmpty());

        WriteCommand cmd;
        cmd.id = QUuid::createUuid();
        cmd.tagId = 1; cmd.value = 100;
        cmd.createdAt = QDateTime::currentDateTime();
        cmd.priority = 0;
        queue.enqueue(cmd);
        QVERIFY(!queue.isEmpty());

        auto dequeued = queue.dequeue();
        QVERIFY(dequeued.has_value());
        QCOMPARE(dequeued->tagId, 1);
        QVERIFY(queue.isEmpty());

        queue.clear();
        QVERIFY(queue.isEmpty());
    }

    void writeQueue_priority_order() {
        WriteQueue queue;
        QDateTime now = QDateTime::currentDateTime();

        for (int i = 0; i < 4; ++i) {
            WriteCommand c; c.id = QUuid::createUuid();
            c.tagId = i; c.value = i;
            c.createdAt = now.addMSecs(i);
            c.priority = (i % 2); // 0,1,0,1
            queue.enqueue(c);
        }

        auto first = queue.dequeue();
        QVERIFY(first.has_value());
        QCOMPARE(first->priority, 1);
        QCOMPARE(first->tagId, 1); // first high priority

        auto second = queue.dequeue();
        QVERIFY(second.has_value());
        QCOMPARE(second->priority, 1);
        QCOMPARE(second->tagId, 3); // second high priority

        queue.clear();
    }

    void history_service_full_roundtrip() {
        QString dbPath = m_tempDir.path() + "/hist_int.db";
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "int_hist");
        db.setDatabaseName(dbPath);
        QVERIFY(db.open());
        { QSqlQuery q(db); q.exec("PRAGMA journal_mode=WAL"); q.finish(); }
        QVERIFY(DatabaseMigrator::migrate(db));

        auto* history = new HistoryService("int_hist");
        for (int i = 0; i < 10; ++i) {
            TagValue tv; tv.tagId = 1; tv.value = (double)i;
            tv.quality = Quality::Good; tv.timestamp = QDateTime::currentDateTime();
            history->enqueueSample(tv);
        }
        history->flush();

        auto results = history->query(1, QDateTime::currentDateTime().addSecs(-10),
                                      QDateTime::currentDateTime().addSecs(10));
        QVERIFY(!results.isEmpty());
        QCOMPARE(results.size(), 10);

        delete history;
        db.close();
        QSqlDatabase::removeDatabase("int_hist");
    }

    void alarm_engine_trigger_and_recover() {
        auto* engine = new AlarmEngine;
        AlarmRule rule; rule.id = 1; rule.tagId = 1; rule.name = "TestAlarm";
        rule.type = AlarmType::High; rule.threshold = 100; rule.delayMs = 0;
        rule.severity = Severity::Warning; rule.enabled = true;
        engine->setRules({rule});

        TagValue tvHigh; tvHigh.tagId = 1; tvHigh.value = 150;
        tvHigh.quality = Quality::Good; tvHigh.timestamp = QDateTime::currentDateTime();
        engine->evaluate(tvHigh);

        QTest::qWait(50);

        delete engine;
    }

    void tagCache_thread_safety() {
        TagCache cache;
        QHash<int, TagValue> initial;
        TagValue tv; tv.tagId = 1; tv.value = 42;
        tv.quality = Quality::Good; tv.timestamp = QDateTime::currentDateTime();
        initial[1] = tv;
        cache.updateValues(initial);

        auto snap = cache.snapshot();
        QVERIFY(snap.contains(1));
        QCOMPARE(snap[1].value.toInt(), 42);
        QCOMPARE(snap[1].quality, Quality::Good);
    }
};

QTEST_MAIN(IntegrationTest)
#include "tst_Integration.moc"
