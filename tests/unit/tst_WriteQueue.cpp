#include <QTest>

#include <optional>

#include "runtime/WriteQueue.h"

class WriteQueueTest : public QObject {
    Q_OBJECT
private slots:
    void emptyQueue_isEmpty_returnsTrue()
    {
        WriteQueue q;
        QVERIFY(q.isEmpty());
    }

    void enqueueDequeue_fifoOrder()
    {
        WriteQueue q;

        WriteCommand a;
        a.tagId = 1;
        a.value = 10;

        WriteCommand b;
        b.tagId = 2;
        b.value = 20;

        q.enqueue(a);
        q.enqueue(b);

        auto first = q.dequeue();
        QVERIFY(first.has_value());
        QCOMPARE(first->tagId, 1);
        QCOMPARE(first->value.toInt(), 10);

        auto second = q.dequeue();
        QVERIFY(second.has_value());
        QCOMPARE(second->tagId, 2);
        QCOMPARE(second->value.toInt(), 20);
    }

    void highPriorityRelease_dequeuedBeforeNormal()
    {
        WriteQueue q;

        WriteCommand normal;
        normal.tagId = 1;
        normal.value = 100;
        normal.isRelease = false;

        WriteCommand release;
        release.tagId = 2;
        release.value = 0;
        release.isRelease = true;

        q.enqueue(normal);
        q.enqueue(release);

        // isRelease commands get priority auto-set to 1 on enqueue.
        auto first = q.dequeue();
        QVERIFY(first.has_value());
        QCOMPARE(first->tagId, 2);
        QVERIFY(first->isRelease);
        QCOMPARE(first->priority, 1);

        auto second = q.dequeue();
        QVERIFY(second.has_value());
        QCOMPARE(second->tagId, 1);
        QCOMPARE(second->priority, 0);
    }

    void samePriority_maintainsFifo()
    {
        WriteQueue q;
        for (int i = 0; i < 5; ++i) {
            WriteCommand c;
            c.tagId = i;
            c.value = i;
            q.enqueue(c);
        }

        for (int i = 0; i < 5; ++i) {
            auto cmd = q.dequeue();
            QVERIFY(cmd.has_value());
            QCOMPARE(cmd->tagId, i);
        }
        QVERIFY(q.isEmpty());
    }

    void removeExpired_removesOnlyExpired()
    {
        WriteQueue q;
        const QDateTime now = QDateTime::currentDateTime();

        WriteCommand expired;
        expired.tagId = 1;
        expired.createdAt = now.addMSecs(-6000);  // 6000ms old > default expiryMs 5000

        WriteCommand fresh;
        fresh.tagId = 2;
        fresh.createdAt = now.addMSecs(-1000);  // 1000ms old <= expiryMs 5000

        q.enqueue(expired);
        q.enqueue(fresh);

        q.removeExpired(now.toMSecsSinceEpoch());

        QVERIFY(!q.isEmpty());
        auto cmd = q.dequeue();
        QVERIFY(cmd.has_value());
        QCOMPARE(cmd->tagId, 2);
        QVERIFY(q.isEmpty());
    }

    void removeExpired_allExpired_emptiesQueue()
    {
        WriteQueue q;
        const QDateTime now = QDateTime::currentDateTime();

        WriteCommand a;
        a.tagId = 1;
        a.createdAt = now.addMSecs(-10000);

        WriteCommand b;
        b.tagId = 2;
        b.createdAt = now.addMSecs(-20000);

        q.enqueue(a);
        q.enqueue(b);

        q.removeExpired(now.toMSecsSinceEpoch());
        QVERIFY(q.isEmpty());
    }

    void clear_dropsAllPending()
    {
        WriteQueue q;

        WriteCommand a;
        a.tagId = 1;
        WriteCommand b;
        b.tagId = 2;
        q.enqueue(a);
        q.enqueue(b);
        QVERIFY(!q.isEmpty());

        q.clear();
        QVERIFY(q.isEmpty());
        QVERIFY(!q.dequeue().has_value());
    }

    void dequeueOnEmpty_returnsNullopt()
    {
        WriteQueue q;
        auto cmd = q.dequeue();
        QVERIFY(!cmd.has_value());
    }
};

QTEST_MAIN(WriteQueueTest)
#include "tst_WriteQueue.moc"
