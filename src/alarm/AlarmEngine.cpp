#include "alarm/AlarmEngine.h"

#include <utility>

AlarmEngine::AlarmEngine(QObject* parent)
    : QObject(parent)
{
}

void AlarmEngine::setRules(QVector<AlarmRule> rules)
{
    m_rules = std::move(rules);

    // 释放旧计时器并重置全部状态
    for (auto& st : m_states) {
        if (st.timer) {
            delete st.timer;
            st.timer = nullptr;
        }
    }
    m_states.clear();
    for (const AlarmRule& r : m_rules) {
        m_states.insert(r.id, RuleState{});
    }
}

void AlarmEngine::evaluate(TagValue tv)
{
    // Stale/Bad/Disconnected 数据不可靠，不参与报警
    if (tv.quality != Quality::Good) {
        return;
    }

    for (const AlarmRule& r : m_rules) {
        if (r.tagId != tv.tagId || !r.enabled) {
            continue;
        }
        RuleState& st = m_states[r.id];
        const double v = tv.value.toDouble();
        st.lastValue = v;

        // Change 报警: 首个采样建立参考值，此后值相对参考值变化即视为条件满足
        if (r.type == AlarmType::Change && !st.hasValue) {
            st.changeRef = v;
            st.hasValue = true;
        }

        stepRule(r, st, conditionMet(r, st, v));
    }
}

void AlarmEngine::acknowledge(int ruleId)
{
    auto it = m_states.find(ruleId);
    if (it == m_states.end()) {
        return;
    }
    RuleState& st = it.value();
    if (st.state == AlarmState::Triggered) {
        st.state = AlarmState::Acknowledged;
    }
}

AlarmState AlarmEngine::stateOf(int ruleId) const
{
    auto it = m_states.constFind(ruleId);
    if (it == m_states.constEnd()) {
        return AlarmState::Normal;
    }
    return it->state;
}

void AlarmEngine::onDelayElapsed(int ruleId)
{
    auto it = m_states.find(ruleId);
    if (it == m_states.end()) {
        return;
    }
    RuleState& st = it.value();
    if (!st.armed || st.state != AlarmState::Pending) {
        return; // 条件已在中途消失（已被取消）或重新武装
    }
    st.armed = false;

    const AlarmRule* r = findRule(ruleId);
    if (!r) {
        st.state = AlarmState::Normal;
        return;
    }
    // 延时结束时条件仍满足才触发
    if (conditionMet(*r, st, st.lastValue)) {
        st.state = AlarmState::Triggered;
        emit alarmTriggered(r->id, r->tagId, st.lastValue);
    } else {
        st.state = AlarmState::Normal;
    }
}

const AlarmRule* AlarmEngine::findRule(int ruleId) const
{
    for (const AlarmRule& r : m_rules) {
        if (r.id == ruleId) {
            return &r;
        }
    }
    return nullptr;
}

bool AlarmEngine::conditionMet(const AlarmRule& r, const RuleState& st, double v) const
{
    switch (r.type) {
    case AlarmType::Bool:
        return v != 0.0;
    case AlarmType::HighHigh:
    case AlarmType::High:
        return v > r.threshold;
    case AlarmType::Low:
    case AlarmType::LowLow:
        return v < r.threshold;
    case AlarmType::Change:
        return st.hasValue && v != st.changeRef;
    }
    return false;
}

bool AlarmEngine::recoveryMet(const AlarmRule& r, const RuleState& st, double v) const
{
    switch (r.type) {
    case AlarmType::Bool:
        return v == 0.0;
    case AlarmType::HighHigh:
    case AlarmType::High:
        return v < r.threshold - r.hysteresis;
    case AlarmType::Low:
    case AlarmType::LowLow:
        return v > r.threshold + r.hysteresis;
    case AlarmType::Change:
        return st.hasValue && v == st.changeRef;
    }
    return false;
}

void AlarmEngine::armDelay(const AlarmRule& r, RuleState& st)
{
    st.delayMs = r.delayMs;
    if (r.delayMs <= 0) {
        // 无延时，立即触发
        st.state = AlarmState::Triggered;
        emit alarmTriggered(r.id, r.tagId, st.lastValue);
        return;
    }
    if (!st.timer) {
        st.timer = new QTimer(this);
        st.timer->setSingleShot(true);
        connect(st.timer, &QTimer::timeout, this, [this, id = r.id] { onDelayElapsed(id); });
    }
    st.armed = true;
    st.timer->start(r.delayMs);
}

void AlarmEngine::cancelDelay(RuleState& st)
{
    st.armed = false;
    if (st.timer) {
        st.timer->stop();
    }
}

void AlarmEngine::stepRule(const AlarmRule& r, RuleState& st, bool cond)
{
    switch (st.state) {
    case AlarmState::Normal:
        if (cond) {
            st.state = AlarmState::Pending;
            armDelay(r, st); // delayMs<=0 时内部直接转 Triggered
        }
        break;
    case AlarmState::Pending:
        if (!cond) {
            cancelDelay(st);
            st.state = AlarmState::Normal;
        }
        break;
    case AlarmState::Triggered:
        if (recoveryMet(r, st, st.lastValue)) {
            cancelDelay(st);
            st.state = AlarmState::Recovered;
            emit alarmRecovered(r.id, r.tagId);
        }
        break;
    case AlarmState::Acknowledged:
        if (recoveryMet(r, st, st.lastValue)) {
            st.state = AlarmState::Recovered;
            emit alarmRecovered(r.id, r.tagId);
        }
        break;
    case AlarmState::Recovered:
        if (cond) {
            st.state = AlarmState::Pending;
            armDelay(r, st);
        }
        break;
    }
}
