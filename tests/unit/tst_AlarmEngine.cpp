#include <QTest>
#include <QSignalSpy>
#include <QDateTime>

#include "alarm/AlarmEngine.h"

class AlarmEngineTest : public QObject {
    Q_OBJECT
private:
    static AlarmRule makeRule(int id, int tagId, AlarmType type, double threshold,
                              int delayMs, double hysteresis = 0)
    {
        AlarmRule r;
        r.id = id;
        r.tagId = tagId;
        r.type = type;
        r.threshold = threshold;
        r.delayMs = delayMs;
        r.hysteresis = hysteresis;
        return r;
    }

    static TagValue sample(int tagId, double value)
    {
        TagValue tv;
        tv.tagId = tagId;
        tv.value = QVariant(value);
        tv.quality = Quality::Good;
        tv.timestamp = QDateTime::currentDateTimeUtc();
        return tv;
    }

private slots:
    void bool_alarm_trigger_ack_recover()
    {
        AlarmEngine engine;
        engine.setRules({ makeRule(1, 1, AlarmType::Bool, 0, 100) });

        QSignalSpy triggered(&engine, &AlarmEngine::alarmTriggered);
        QSignalSpy recovered(&engine, &AlarmEngine::alarmRecovered);

        engine.evaluate(sample(1, 0));
        QCOMPARE(triggered.count(), 0);
        QCOMPARE(engine.stateOf(1), AlarmState::Normal);

        // 值变 1，进入延时待触发
        engine.evaluate(sample(1, 1));
        QCOMPARE(triggered.count(), 0);
        QCOMPARE(engine.stateOf(1), AlarmState::Pending);

        // 延时到，触发
        QTest::qWait(150);
        QCOMPARE(triggered.count(), 1);
        QCOMPARE(triggered.at(0).at(0).toInt(), 1);          // ruleId
        QCOMPARE(triggered.at(0).at(1).toInt(), 1);          // tagId
        QCOMPARE(triggered.at(0).at(2).toDouble(), 1.0);     // 触发值
        QCOMPARE(engine.stateOf(1), AlarmState::Triggered);

        // ack → Acknowledged
        engine.acknowledge(1);
        QCOMPARE(engine.stateOf(1), AlarmState::Acknowledged);

        // 值变 0 → 恢复
        engine.evaluate(sample(1, 0));
        QCOMPARE(recovered.count(), 1);
        QCOMPARE(recovered.at(0).at(0).toInt(), 1);
        QCOMPARE(recovered.at(0).at(1).toInt(), 1);
        QCOMPARE(engine.stateOf(1), AlarmState::Recovered);

        // 恢复后再次变化可重新触发
        engine.evaluate(sample(1, 1));
        QTest::qWait(150);
        QCOMPARE(triggered.count(), 2);
    }

    void high_alarm_triggers_and_recovery_uses_hysteresis()
    {
        AlarmEngine engine;
        engine.setRules({ makeRule(2, 1, AlarmType::High, 100.0, 0, 10.0) });

        QSignalSpy triggered(&engine, &AlarmEngine::alarmTriggered);
        QSignalSpy recovered(&engine, &AlarmEngine::alarmRecovered);

        // 超过阈值，无延时立即触发
        engine.evaluate(sample(1, 150));
        QCOMPARE(triggered.count(), 1);
        QCOMPARE(engine.stateOf(2), AlarmState::Triggered);

        // 降到阈值附近但不低于 threshold-hysteresis(90)：不恢复
        engine.evaluate(sample(1, 100));
        QCOMPARE(recovered.count(), 0);
        engine.evaluate(sample(1, 95));
        QCOMPARE(recovered.count(), 0);

        // 低于 90 → 恢复
        engine.evaluate(sample(1, 89));
        QCOMPARE(recovered.count(), 1);
        QCOMPARE(engine.stateOf(2), AlarmState::Recovered);
    }

    void low_alarm_triggers_below_threshold()
    {
        AlarmEngine engine;
        engine.setRules({ makeRule(3, 1, AlarmType::Low, 0.0, 0) });

        QSignalSpy triggered(&engine, &AlarmEngine::alarmTriggered);

        engine.evaluate(sample(1, 5));
        QCOMPARE(triggered.count(), 0);

        engine.evaluate(sample(1, -3));
        QCOMPARE(triggered.count(), 1);
        QCOMPARE(triggered.at(0).at(2).toDouble(), -3.0);
        QCOMPARE(engine.stateOf(3), AlarmState::Triggered);
    }

    void condition_cleared_during_delay_does_not_trigger()
    {
        AlarmEngine engine;
        engine.setRules({ makeRule(4, 1, AlarmType::High, 100.0, 200) });

        QSignalSpy triggered(&engine, &AlarmEngine::alarmTriggered);

        engine.evaluate(sample(1, 150));
        QCOMPARE(engine.stateOf(4), AlarmState::Pending);

        // 延时内条件消失
        engine.evaluate(sample(1, 50));
        QCOMPARE(engine.stateOf(4), AlarmState::Normal);

        QTest::qWait(300); // 远超 200ms 延时
        QCOMPARE(triggered.count(), 0);
    }

    void disabled_rule_never_triggers()
    {
        AlarmEngine engine;
        AlarmRule r = makeRule(5, 1, AlarmType::Bool, 0, 0);
        r.enabled = false;
        engine.setRules({ r });

        QSignalSpy triggered(&engine, &AlarmEngine::alarmTriggered);

        engine.evaluate(sample(1, 1));
        QCOMPARE(triggered.count(), 0);
        QCOMPARE(engine.stateOf(5), AlarmState::Normal);
    }

    void change_alarm_triggers_on_value_change()
    {
        AlarmEngine engine;
        engine.setRules({ makeRule(6, 1, AlarmType::Change, 0, 0) });

        QSignalSpy triggered(&engine, &AlarmEngine::alarmTriggered);
        QSignalSpy recovered(&engine, &AlarmEngine::alarmRecovered);

        // 首个采样仅建立参考值，不触发
        engine.evaluate(sample(1, 0));
        QCOMPARE(triggered.count(), 0);

        // 相对参考值变化 → 触发
        engine.evaluate(sample(1, 5));
        QCOMPARE(triggered.count(), 1);
        QCOMPARE(triggered.at(0).at(2).toDouble(), 5.0);

        // 值保持，不恢复
        engine.evaluate(sample(1, 5));
        QCOMPARE(recovered.count(), 0);

        engine.acknowledge(6);
        QCOMPARE(engine.stateOf(6), AlarmState::Acknowledged);

        // 回到参考值 → 恢复
        engine.evaluate(sample(1, 0));
        QCOMPARE(recovered.count(), 1);
        QCOMPARE(engine.stateOf(6), AlarmState::Recovered);
    }

    void highhigh_and_lowlow_share_threshold_logic()
    {
        AlarmEngine engine;
        engine.setRules({
            makeRule(7, 2, AlarmType::HighHigh, 200.0, 0),
            makeRule(8, 2, AlarmType::LowLow, -100.0, 0),
        });

        QSignalSpy triggered(&engine, &AlarmEngine::alarmTriggered);

        engine.evaluate(sample(2, 250)); // HighHigh 触发
        QCOMPARE(triggered.count(), 1);
        QCOMPARE(triggered.at(0).at(0).toInt(), 7);

        engine.evaluate(sample(2, -150)); // LowLow 触发
        QCOMPARE(triggered.count(), 2);
        QCOMPARE(triggered.at(1).at(0).toInt(), 8);
    }

    void non_good_quality_values_are_ignored()
    {
        AlarmEngine engine;
        engine.setRules({ makeRule(9, 1, AlarmType::High, 100.0, 0) });

        QSignalSpy triggered(&engine, &AlarmEngine::alarmTriggered);

        TagValue tv = sample(1, 150);
        tv.quality = Quality::Bad;
        engine.evaluate(tv);
        QCOMPARE(triggered.count(), 0);
        QCOMPARE(engine.stateOf(9), AlarmState::Normal);

        engine.evaluate(sample(1, 150));
        QCOMPARE(triggered.count(), 1);
    }

    void acknowledge_on_inactive_rule_is_noop()
    {
        AlarmEngine engine;
        engine.setRules({ makeRule(10, 1, AlarmType::High, 100.0, 0) });

        QSignalSpy triggered(&engine, &AlarmEngine::alarmTriggered);

        // 未触发时 ack 无副作用
        engine.acknowledge(10);
        QCOMPARE(engine.stateOf(10), AlarmState::Normal);

        engine.evaluate(sample(1, 200));
        QCOMPARE(triggered.count(), 1);
    }
};

QTEST_MAIN(AlarmEngineTest)
#include "tst_AlarmEngine.moc"
