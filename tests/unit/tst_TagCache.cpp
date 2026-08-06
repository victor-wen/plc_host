#include <QTest>
#include "runtime/TagCache.h"
#include "domain/TagValue.h"

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
};

QTEST_MAIN(TagCacheTest)
#include "tst_TagCache.moc"
