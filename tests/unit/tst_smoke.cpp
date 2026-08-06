#include <QTest>

class SmokeTest : public QObject {
    Q_OBJECT
private slots:
    void alwaysPasses()
    {
        QVERIFY(true);
    }
};

QTEST_MAIN(SmokeTest)
#include "tst_smoke.moc"
