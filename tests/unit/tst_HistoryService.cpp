#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>

#include "storage/DatabaseMigrator.h"
#include "history/HistoryService.h"

class HistoryServiceTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tempDir;
    int m_connCounter = 0;

    struct DbFixture {
        QString connName;
        QString path;
        QSqlDatabase db;
    };

    // 建立独立连接（每测试唯一名称），WAL + 迁移到最新 schema
    DbFixture openDb(const QString& suffix)
    {
        DbFixture f;
        f.connName = QStringLiteral("history_%1_%2").arg(++m_connCounter).arg(suffix);
        f.path = m_tempDir.path() + "/" + f.connName + ".db";
        f.db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), f.connName);
        f.db.setDatabaseName(f.path);
        f.db.open();
        {
            // PRAGMA 语句必须先行结束，否则会阻塞后续迁移事务的 COMMIT
            QSqlQuery q(f.db);
            q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        }
        DatabaseMigrator::migrate(f.db);
        return f;
    }

private slots:
    void enqueue_flush_query_roundtrip()
    {
        DbFixture d = openDb("roundtrip");
        QVERIFY(d.db.isOpen());
        QCOMPARE(DatabaseMigrator::currentVersion(d.db), 1);

        HistoryService svc(d.connName);

        const QDateTime now = QDateTime::currentDateTimeUtc();
        TagValue tv;
        tv.tagId = 1;
        tv.value = QVariant(42.5);
        tv.quality = Quality::Good;
        tv.timestamp = now;

        svc.enqueueSample(tv);

        // 未 flush 前查询为空
        QVERIFY(svc.query(1, now.addSecs(-60), now.addSecs(60)).isEmpty());

        svc.flush();

        const auto rows = svc.query(1, now.addSecs(-60), now.addSecs(60));
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].tagId, 1);
        QCOMPARE(rows[0].value.toDouble(), 42.5);
        QVERIFY(rows[0].quality == Quality::Good);
        QCOMPARE(rows[0].timestamp.toMSecsSinceEpoch(), now.toMSecsSinceEpoch());

        // 空缓冲 flush 不产生重复数据
        svc.flush();
        QCOMPARE(svc.query(1, now.addSecs(-60), now.addSecs(60)).size(), 1);
    }

    void batch_write_auto_flushes()
    {
        DbFixture d = openDb("batch");
        QVERIFY(d.db.isOpen());
        HistoryService svc(d.connName);

        constexpr int batchSize = 500; // HistoryService::kBatchSize
        const QDateTime base = QDateTime::currentDateTimeUtc().addSecs(-batchSize);
        for (int i = 0; i < batchSize; ++i) {
            TagValue tv;
            tv.tagId = 1;
            tv.value = QVariant(double(i));
            tv.timestamp = base.addSecs(i);
            svc.enqueueSample(tv);
        }

        // 第 500 条触发自动 flush，缓冲已清空
        QCOMPARE(svc.query(1, base, base.addSecs(batchSize + 10)).size(), batchSize);

        // 再入队 3 条，仍在缓冲，查询不受影响
        for (int i = 0; i < 3; ++i) {
            TagValue tv;
            tv.tagId = 1;
            tv.value = QVariant(1000.0 + i);
            tv.timestamp = base.addSecs(batchSize + i);
            svc.enqueueSample(tv);
        }
        QCOMPARE(svc.query(1, base, base.addSecs(batchSize + 10)).size(), batchSize);

        // 手动 flush 后 3 条落库
        svc.flush();
        QCOMPARE(svc.query(1, base, base.addSecs(batchSize + 10)).size(), batchSize + 3);
    }

    void query_empty_returns_empty()
    {
        DbFixture d = openDb("empty");
        QVERIFY(d.db.isOpen());
        HistoryService svc(d.connName);

        const QDateTime now = QDateTime::currentDateTimeUtc();
        TagValue tv;
        tv.tagId = 1;
        tv.value = QVariant(1.0);
        tv.timestamp = now;
        svc.enqueueSample(tv);
        svc.flush();

        // 时间范围不覆盖任何样本
        const QDateTime later = now.addSecs(3600);
        QVERIFY(svc.query(1, later, later).isEmpty());

        // 其他 tag 无数据
        QVERIFY(svc.query(99, now.addSecs(-3600), now.addSecs(3600)).isEmpty());
    }

    void cleanOldData_deletes_only_expired()
    {
        DbFixture d = openDb("clean");
        QVERIFY(d.db.isOpen());
        HistoryService svc(d.connName);

        const QDateTime now = QDateTime::currentDateTimeUtc();
        for (int i = 0; i < 5; ++i) {
            TagValue tv;
            tv.tagId = 1;
            tv.value = QVariant(double(i));
            tv.timestamp = now.addDays(-100).addSecs(i);
            svc.enqueueSample(tv);
        }
        for (int i = 0; i < 3; ++i) {
            TagValue tv;
            tv.tagId = 1;
            tv.value = QVariant(10.0 + i);
            tv.timestamp = now.addDays(-10).addSecs(i);
            svc.enqueueSample(tv);
        }
        svc.flush();
        QCOMPARE(svc.query(1, now.addDays(-110), now.addDays(-8)).size(), 8);

        svc.cleanOldData(90);

        // 100 天前的数据被清除
        QVERIFY(svc.query(1, now.addDays(-110), now.addDays(-90)).isEmpty());
        // 10 天前的数据保留
        QCOMPARE(svc.query(1, now.addDays(-11), now.addDays(-9)).size(), 3);
    }

    void cleanOldData_batches_many_rows()
    {
        DbFixture d = openDb("cleanbatch");
        QVERIFY(d.db.isOpen());
        HistoryService svc(d.connName);

        const QDateTime now = QDateTime::currentDateTimeUtc();
        const int oldCount = 2500;
        for (int i = 0; i < oldCount; ++i) {
            TagValue tv;
            tv.tagId = 1;
            tv.value = QVariant(double(i));
            tv.timestamp = now.addDays(-200).addSecs(i);
            svc.enqueueSample(tv);
        }
        svc.flush();
        QCOMPARE(svc.query(1, now.addDays(-201), now.addDays(-199)).size(), oldCount);

        // 分批清理（每批 1000 行，2500 行需要 3 批）
        svc.cleanOldData(90);

        QVERIFY(svc.query(1, now.addDays(-201), now.addDays(-199)).isEmpty());
    }
};

QTEST_MAIN(HistoryServiceTest)
#include "tst_HistoryService.moc"
