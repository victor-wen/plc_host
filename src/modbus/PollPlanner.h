#pragma once

#include <QModbusDataUnit>
#include <QVector>

#include "domain/Tag.h"

// 一个轮询组 = 一次 Modbus 读请求覆盖的连续寄存器块（CORE-04）。
// 块内 tag 全部满足：同 registerType、同 pollIntervalMs、地址连续（间隔 <= 5）、
// 且块寄存器数不超过 125。
struct PollGroup {
    QModbusDataUnit::RegisterType registerType = QModbusDataUnit::HoldingRegisters;
    int startAddress = 0;   // 零基起始地址
    int count = 0;          // 块内寄存器数（含 gap，<= 125）
    int intervalMs = 0;     // 本组轮询周期
    QVector<int> tagIds;    // 本组覆盖的 tag id（按地址升序）
};

// 纯函数式规划器，无内部状态；可在任意线程使用。
class PollPlanner {
public:
    // 由全部 tag 列表生成轮询组。空输入返回空列表。
    QVector<PollGroup> buildGroups(const QVector<Tag>& tags);
};
