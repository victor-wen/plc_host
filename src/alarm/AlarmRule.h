#pragma once

#include <QString>

// 报警类型。枚举值对应数据库 alarm_rules.alarm_type 的存储值。
enum class AlarmType {
    Bool = 0,     // 数字量：非零即报警
    HighHigh = 1, // 高高报：值 > threshold
    High = 2,     // 高报：值 > threshold
    Low = 3,      // 低报：值 < threshold
    LowLow = 4,   // 低低报：值 < threshold
    Change = 5    // 变化报：值相对参考值发生变化
};

// 报警级别。枚举值对应数据库 alarm_rules.severity 的存储值。
enum class Severity {
    Info = 0,
    Warning = 1,
    Critical = 2
};

struct AlarmRule {
    int id = -1;
    int tagId = -1;
    QString name;
    AlarmType type = AlarmType::Bool;
    double threshold = 0;
    int delayMs = 0;       // 条件持续满足的确认延时
    double hysteresis = 0; // 恢复回差：高报警需低于 threshold-hysteresis 才恢复
    Severity severity = Severity::Warning;
    bool enabled = true;
};
