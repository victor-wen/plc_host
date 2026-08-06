#include <QTest>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QRandomGenerator>
#include <limits>

#include "runtime/TagCache.h"
#include "domain/TagValue.h"

namespace {
constexpr int kConcurrentTagCount = 100;
} // namespace

// 写线程: 循环 updateValues。每个迭代确定性地写一个 tag (保证全覆盖 1..kConcurrentTagCount),
// 再混入 2..4 个随机 tag 制造读写交叠。
class WriterWorker : public QObject {
    Q_OBJECT
public:
    explicit WriterWorker(TagCache* cache, quint32 seed, int iterations)
        : QObject(nullptr), m_cache(cache), m_seed(seed), m_iterations(iterations) {}

public slots:
    void run()
    {
        QRandomGenerator rng(m_seed);
        for (int i = 0; i < m_iterations; ++i) {
            QHash<int, TagValue> batch;

            const int fixedTag = (i % kConcurrentTagCount) + 1;   // 确定性覆盖全部 tagId
            TagValue v;
            v.tagId = fixedTag;
            v.value = QVariant(double(i));
            v.rawValue = QVariant(double(i));
            v.quality = Quality::Good;
            v.timestamp = QDateTime::currentDateTime();
            batch.insert(fixedTag, v);

            const int extras = 2 + int(rng.bounded(quint32(3)));  // 2..4 个随机 tag
            for (int e = 0; e < extras; ++e) {
                const int id = 1 + int(rng.bounded(quint32(kConcurrentTagCount)));
                TagValue rv;
                rv.tagId = id;
                rv.value = QVariant(double(i));
                rv.quality = Quality::Good;
                rv.timestamp = QDateTime::currentDateTime();
                batch.insert(id, rv);
            }
            m_cache->updateValues(batch);

            if ((i & 0x3F) == 0)      // 周期性让出 CPU, 提升读写交叠概率
                QThread::msleep(1);
        }
        emit finished();
    }

signals:
    void finished();

private:
    TagCache* m_cache;
    quint32 m_seed;
    int m_iterations;
};

// 读线程: 循环 snapshot() + staleTagIds(), 并记录观察到的快照大小范围
// (主线程在 wait() 后读取, 线程 join 建立 happens-before, 无数据竞争)。
class ReaderWorker : public QObject {
    Q_OBJECT
public:
    explicit ReaderWorker(TagCache* cache, int iterations)
        : QObject(nullptr), minSize(std::numeric_limits<int>::max()), maxSize(-1),
          m_cache(cache), m_iterations(iterations) {}

    int minSize;
    int maxSize;

public slots:
    void run()
    {
        for (int i = 0; i < m_iterations; ++i) {
            const auto snap = m_cache->snapshot();
            const int s = int(snap.size());
            minSize = qMin(minSize, s);
            maxSize = qMax(maxSize, s);
            const auto stale = m_cache->staleTagIds(500);
            Q_UNUSED(stale);
            if ((i & 0x3F) == 0)
                QThread::msleep(1);
        }
        emit finished();
    }

signals:
    void finished();

private:
    TagCache* m_cache;
    int m_iterations;
};

class TagCacheTest : public QObject {
    Q_OBJECT
private slots:
    void emptyCache_snapshot_returnsEmptyHash()
    {
        TagCache cache;
        QVERIFY(cache.snapshot().isEmpty());
    }

    void updateValues_snapshot_containsValuesWithQuality()
    {
        TagCache cache;

        TagValue v;
        v.tagId = 1;
        v.value = QVariant(42.5);
        v.rawValue = QVariant(42);
        v.quality = Quality::Good;
        v.timestamp = QDateTime::currentDateTime();

        QHash<int, TagValue> in;
        in.insert(1, v);
        cache.updateValues(in);

        auto snap = cache.snapshot();
        QCOMPARE(snap.size(), 1);
        QVERIFY(snap.contains(1));
        QCOMPARE(snap[1].tagId, 1);
        QCOMPARE(snap[1].value, v.value);
        QCOMPARE(snap[1].rawValue, v.rawValue);
        QCOMPARE(snap[1].quality, Quality::Good);
        QCOMPARE(snap[1].timestamp, v.timestamp);
    }

    void partialUpdate_keepsUnupdatedValues()
    {
        TagCache cache;

        TagValue a;
        a.tagId = 1;
        a.quality = Quality::Good;

        TagValue b;
        b.tagId = 2;
        b.quality = Quality::Good;

        QHash<int, TagValue> initial;
        initial.insert(1, a);
        initial.insert(2, b);
        cache.updateValues(initial);

        TagValue bUpdated;
        bUpdated.tagId = 2;
        bUpdated.quality = Quality::Stale;

        QHash<int, TagValue> partial;
        partial.insert(2, bUpdated);
        cache.updateValues(partial);

        auto snap = cache.snapshot();
        QCOMPARE(snap.size(), 2);
        QVERIFY(snap.contains(1));                    // not in update, must survive
        QCOMPARE(snap[1].quality, Quality::Good);     // unchanged
        QCOMPARE(snap[2].quality, Quality::Stale);    // updated
    }

    void staleTagIds_identifiesOldTimestamps()
    {
        TagCache cache;
        const QDateTime now = QDateTime::currentDateTime();

        TagValue old;
        old.tagId = 10;
        old.timestamp = now.addMSecs(-2000);

        TagValue fresh;
        fresh.tagId = 11;
        fresh.timestamp = now.addMSecs(-100);

        TagValue neverUpdated;      // invalid timestamp -> treated as stale
        neverUpdated.tagId = 12;

        QHash<int, TagValue> in;
        in.insert(10, old);
        in.insert(11, fresh);
        in.insert(12, neverUpdated);
        cache.updateValues(in);

        const auto stale = cache.staleTagIds(500);
        QVERIFY(stale.contains(10));   // 2000ms old > 500ms threshold
        QVERIFY(!stale.contains(11));  // 100ms old <= 500ms threshold
        QVERIFY(stale.contains(12));   // never stamped -> stale
    }

    void value_returnsDefault_forUnknownTagId()
    {
        TagCache cache;
        const TagValue v = cache.value(999);
        QCOMPARE(v.tagId, -1);
        QCOMPARE(v.quality, Quality::Disconnected);
        QVERIFY(v.value.isNull());
        QVERIFY(!v.timestamp.isValid());
    }

    // CORE-06: 2 写线程 + 2 读线程并发压力测试。
    void concurrentReadWrite()
    {
        TagCache cache;
        constexpr int kIterations = 500;

        WriterWorker w1(&cache, 0x1111u, kIterations);
        WriterWorker w2(&cache, 0x2222u, kIterations);
        ReaderWorker r1(&cache, kIterations);
        ReaderWorker r2(&cache, kIterations);

        QThread t1;
        QThread t2;
        QThread t3;
        QThread t4;

        w1.moveToThread(&t1);
        w2.moveToThread(&t2);
        r1.moveToThread(&t3);
        r2.moveToThread(&t4);

        QObject::connect(&t1, &QThread::started, &w1, &WriterWorker::run);
        QObject::connect(&t2, &QThread::started, &w2, &WriterWorker::run);
        QObject::connect(&t3, &QThread::started, &r1, &ReaderWorker::run);
        QObject::connect(&t4, &QThread::started, &r2, &ReaderWorker::run);

        // worker 完成后通知对应线程退出事件循环, 否则 QThread::wait() 会永久阻塞。
        QObject::connect(&w1, &WriterWorker::finished, &t1, &QThread::quit);
        QObject::connect(&w2, &WriterWorker::finished, &t2, &QThread::quit);
        QObject::connect(&r1, &ReaderWorker::finished, &t3, &QThread::quit);
        QObject::connect(&r2, &ReaderWorker::finished, &t4, &QThread::quit);

        t1.start();
        t2.start();
        t3.start();
        t4.start();

        // 信号通知: 全部 worker 的 finished 后继续。
        int completed = 0;
        QEventLoop loop;
        const auto onFinished = [&loop, &completed] {
            if (++completed == 4)
                loop.quit();
        };
        QObject::connect(&w1, &WriterWorker::finished, &loop, onFinished);
        QObject::connect(&w2, &WriterWorker::finished, &loop, onFinished);
        QObject::connect(&r1, &ReaderWorker::finished, &loop, onFinished);
        QObject::connect(&r2, &ReaderWorker::finished, &loop, onFinished);

        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        watchdog.start(30000);
        loop.exec();

        QCOMPARE(completed, 4);       // 4 个 worker 都通过信号报告完成
        QVERIFY(t1.wait(30000));      // 兜底: 线程全部退出
        QVERIFY(t2.wait(30000));
        QVERIFY(t3.wait(30000));
        QVERIFY(t4.wait(30000));

        // 无崩溃 + 快照大小始终在合法区间 (0..kConcurrentTagCount)。
        const auto snap = cache.snapshot();
        QVERIFY(snap.size() >= 0);                            // 需求: hash 大小 >= 0
        QCOMPARE(int(snap.size()), kConcurrentTagCount);      // 确定性覆盖全部 tagId, 无丢失更新
        QVERIFY(r1.minSize >= 0 && r1.maxSize <= kConcurrentTagCount);
        QVERIFY(r2.minSize >= 0 && r2.maxSize <= kConcurrentTagCount);
        for (auto it = snap.cbegin(); it != snap.cend(); ++it) {
            QCOMPARE(it.value().quality, Quality::Good);
            QVERIFY(it.value().timestamp.isValid());
        }
    }

    // CORE-06: snapshot 是全量拷贝, 后续写不影响已获取的快照。
    void snapshotIsolation()
    {
        TagCache cache;

        TagValue v1;
        v1.tagId = 1;
        v1.value = QVariant(100);
        v1.quality = Quality::Good;
        v1.timestamp = QDateTime::currentDateTime();

        QHash<int, TagValue> in1;
        in1.insert(1, v1);
        cache.updateValues(in1);

        const auto snap = cache.snapshot();
        QCOMPARE(snap[1].value, QVariant(100));

        TagValue v2 = v1;
        v2.value = QVariant(200);
        QHash<int, TagValue> in2;
        in2.insert(1, v2);
        cache.updateValues(in2);

        // 之前获取的快照保持旧值 (值语义拷贝, 与缓存无关)。
        QCOMPARE(snap[1].value, QVariant(100));
        // 缓存本身已更新。
        QCOMPARE(cache.value(1).value, QVariant(200));
    }

    // CORE-06: timestamp 恰好等于 (now - thresholdMs) 时不判 stale (实现用 > 而非 >=)。
    void staleBoundary()
    {
        TagCache cache;
        constexpr int kThresholdMs = 500;

        // staleTagIds 内部会重新读取 now, 其毫秒刻度可能与外部构造时刻不同:
        // 记录两次刻度, 同一刻度(常态)时内部 delta 精确等于阈值, 可确定性断言边界语义。
        const qint64 t0 = QDateTime::currentMSecsSinceEpoch();

        TagValue boundary;
        boundary.tagId = 1;
        boundary.quality = Quality::Good;
        boundary.timestamp = QDateTime::fromMSecsSinceEpoch(t0 - kThresholdMs);

        QHash<int, TagValue> in;
        in.insert(1, boundary);
        cache.updateValues(in);

        const auto stale = cache.staleTagIds(kThresholdMs);
        const qint64 t1 = QDateTime::currentMSecsSinceEpoch();

        if (t1 == t0) {
            QVERIFY(!stale.contains(1));   // delta == kThresholdMs: 500 > 500 为 false
        } else {
            QSKIP("毫秒刻度推进: 内部 delta == kThresholdMs+1, 无法复现精确边界");
        }
    }
};

QTEST_MAIN(TagCacheTest)
#include "tst_TagCache.moc"
