#include "modbus/PollPlanner.h"

#include <algorithm>

namespace {

// 域模型 RegisterType -> QModbusDataUnit::RegisterType 映射。
QModbusDataUnit::RegisterType toModbusRegisterType(RegisterType type)
{
    switch (type) {
    case RegisterType::Coil:
        return QModbusDataUnit::Coils;
    case RegisterType::DiscreteInput:
        return QModbusDataUnit::DiscreteInputs;
    case RegisterType::InputRegister:
        return QModbusDataUnit::InputRegisters;
    case RegisterType::HoldingRegister:
        return QModbusDataUnit::HoldingRegisters;
    }
    return QModbusDataUnit::HoldingRegisters;
}

// Modbus PDU 单请求寄存器数上限。
constexpr int kMaxRegistersPerBlock = 125;
// 相邻 tag 地址间隔 <= 5 个寄存器时合并为一个读取块。
constexpr int kMaxMergeGap = 5;

} // namespace

QVector<PollGroup> PollPlanner::buildGroups(const QVector<Tag>& tags)
{
    QVector<PollGroup> groups;
    if (tags.isEmpty())
        return groups;

    // 按 (registerType, pollIntervalMs, address) 升序排序，保证：
    //   - 不同 registerType / intervalMs 的 tag 天然分离（规则 1、5）；
    //   - 组内按 address 升序（规则 2），tagIds 也自然按地址升序。
    QVector<Tag> sorted = tags;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Tag& a, const Tag& b) {
                         if (a.registerType != b.registerType)
                             return int(a.registerType) < int(b.registerType);
                         if (a.pollIntervalMs != b.pollIntervalMs)
                             return a.pollIntervalMs < b.pollIntervalMs;
                         return a.address < b.address;
                     });

    PollGroup current;
    bool hasCurrent = false;
    int currentEnd = -1; // 当前块覆盖的最后一个寄存器地址（含 gap）

    for (const Tag& tag : sorted) {
        const QModbusDataUnit::RegisterType modbusType = toModbusRegisterType(tag.registerType);
        const int tagEnd = tag.address + tag.registerCount() - 1;

        const bool sameKey = hasCurrent
            && current.registerType == modbusType
            && current.intervalMs == tag.pollIntervalMs;
        const int gap = tag.address - currentEnd - 1;
        const int mergedCount = tagEnd - current.startAddress + 1;

        if (sameKey && gap <= kMaxMergeGap && mergedCount <= kMaxRegistersPerBlock) {
            // 合并到当前块（规则 3、4）
            current.count = mergedCount;
            currentEnd = tagEnd;
            current.tagIds.append(tag.id);
        } else {
            if (hasCurrent)
                groups.append(current);
            current = PollGroup{};
            current.registerType = modbusType;
            current.startAddress = tag.address;
            current.count = tag.registerCount();
            current.intervalMs = tag.pollIntervalMs;
            current.tagIds = {tag.id};
            currentEnd = tagEnd;
            hasCurrent = true;
        }
    }

    if (hasCurrent)
        groups.append(current);

    return groups;
}
