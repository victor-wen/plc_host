#pragma once

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QVector>

#include "alarm/AlarmRule.h"
#include "domain/TagValue.h"

// 报警状态机状态
enum class AlarmState {
    Normal,       // 无报警条件
    Pending,      // 条件已满足，等待延时确认
    Triggered,    // 延时确认触发（未确认）
    Acknowledged, // 已确认，等待条件消除
    Recovered     // 条件消除，已恢复
};

// 报警引擎：由通信线程持有，所有方法均在该线程调用。
// 状态机: Normal ->(条件满足+delay) Pending ->(延时到) Triggered ->(ack) Acknowledged ->(条件消除) Recovered
// 恢复回差: 高报警需低于 threshold-hysteresis，低报警需高于 threshold+hysteresis。
// 仅 Quality::Good 的采样参与评估。
class AlarmEngine : public QObject {
    Q_OBJECT
public:
    explicit AlarmEngine(QObject* parent = nullptr);

    // 替换全部规则；内部状态（含延时计时器）一并重置
    void setRules(QVector<AlarmRule> rules);

    // 评估一个采样值，检查所有 tag_id 匹配且启用的规则
    void evaluate(TagValue tv);

    // 确认报警：仅对 Triggered 状态生效，转移到 Acknowledged
    void acknowledge(int ruleId);

    // 查询规则当前状态（供 UI/测试）
    AlarmState stateOf(int ruleId) const;

signals:
    void alarmTriggered(int ruleId, int tagId, double value);
    void alarmRecovered(int ruleId, int tagId);

private slots:
    void onDelayElapsed(int ruleId);

private:
    struct RuleState {
        AlarmState state = AlarmState::Normal;
        bool armed = false;    // 延时计时中
        bool hasValue = false; // 是否已收到首个采样
        double lastValue = 0;  // 最近一次评估值
        double changeRef = 0;  // Change 报警参考值（变化前的值）
        int delayMs = 0;
        QTimer* timer = nullptr;
    };

    const AlarmRule* findRule(int ruleId) const;
    bool conditionMet(const AlarmRule& r, const RuleState& st, double v) const;
    bool recoveryMet(const AlarmRule& r, const RuleState& st, double v) const;
    void armDelay(const AlarmRule& r, RuleState& st);
    void cancelDelay(RuleState& st);
    void stepRule(const AlarmRule& r, RuleState& st, bool cond);

    QVector<AlarmRule> m_rules;
    QHash<int, RuleState> m_states; // key: ruleId
};
