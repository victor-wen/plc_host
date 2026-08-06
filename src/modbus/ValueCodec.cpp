#include "modbus/ValueCodec.h"

#include <QDebug>

#include <bit>
#include <cmath>
#include <utility>

namespace {

quint16 swapBytes(quint16 word)
{
    return quint16((word >> 8) | (word << 8));
}

QModbusDataUnit::RegisterType toModbusType(RegisterType type)
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
    return QModbusDataUnit::Invalid;
}

bool isDefaultScaling(const Tag& tag)
{
    return tag.scale == 1.0 && tag.offset == 0.0;
}

// 原始值 → 工程值（decode 用）。scale=1 且 offset=0 时保持原始类型。
QVariant toEngineering(const QVariant& raw, const Tag& tag)
{
    if (isDefaultScaling(tag))
        return raw;
    return QVariant(raw.toDouble() * tag.scale + tag.offset);
}

// 工程值 → 原始值（encode 用）
double toRaw(double engineering, const Tag& tag)
{
    return (engineering - tag.offset) / tag.scale;
}

// 按字节序将 32 位值拆为两个寄存器字（encode 侧）
std::pair<quint16, quint16> splitWords(quint32 value, ByteOrder order)
{
    const quint16 high = quint16(value >> 16);
    const quint16 low = quint16(value & 0xFFFFu);
    switch (order) {
    case ByteOrder::ABCD: // 高字在前、字内字节不交换
        return {high, low};
    case ByteOrder::DCBA: // 低字在前、字内字节不交换
        return {low, high};
    case ByteOrder::BADC: // 高字在前、字内字节交换
        return {swapBytes(high), swapBytes(low)};
    case ByteOrder::CDAB: // 低字在前、字内字节交换
        return {swapBytes(low), swapBytes(high)};
    }
    return {high, low};
}

// 按字节序将两个寄存器字合并为 32 位值（decode 侧）
quint32 mergeWords(quint16 word0, quint16 word1, ByteOrder order)
{
    switch (order) {
    case ByteOrder::ABCD:
        return (quint32(word0) << 16) | word1;
    case ByteOrder::DCBA:
        return (quint32(word1) << 16) | word0;
    case ByteOrder::BADC:
        return (quint32(swapBytes(word0)) << 16) | swapBytes(word1);
    case ByteOrder::CDAB:
        return (quint32(swapBytes(word1)) << 16) | swapBytes(word0);
    }
    return (quint32(word0) << 16) | word1;
}

} // namespace

DecodeResult ValueCodec::decode(const QModbusDataUnit& unit, const Tag& tag)
{
    DecodeResult result;

    if (tag.address < 0) {
        result.error = QStringLiteral("tag address %1 is negative").arg(tag.address);
        return result;
    }

    const int registerCount = tag.registerCount();
    const int start = unit.startAddress();
    const int count = unit.valueCount();

    // 地址越界：tag 所需寄存器块必须完全落在读单元范围内
    if (tag.address < start || tag.address + registerCount > start + count) {
        result.error = QStringLiteral("tag address %1 needs %2 register(s), outside read unit [%3, %4)")
                           .arg(tag.address).arg(registerCount).arg(start).arg(start + count);
        return result;
    }

    const QVector<quint16>& values = unit.values();
    const int offset = tag.address - start;
    if (offset + registerCount > values.size()) {
        result.error = QStringLiteral("reply contains only %1 register(s), need %2 at offset %3")
                           .arg(values.size()).arg(registerCount).arg(offset);
        return result;
    }

    switch (tag.dataType) {
    case DataType::Bool: {
        bool bit = false;
        if (tag.registerType == RegisterType::Coil || tag.registerType == RegisterType::DiscreteInput)
            bit = values[offset] != 0;
        else
            bit = (values[offset] >> tag.bitPosition) & 1u;
        result.value = QVariant(bit);
        break;
    }
    case DataType::Int16: {
        const qint16 raw = static_cast<qint16>(values[offset]);
        result.value = toEngineering(QVariant(int(raw)), tag);
        break;
    }
    case DataType::UInt16: {
        result.value = toEngineering(QVariant(values[offset]), tag);
        break;
    }
    case DataType::Int32:
    case DataType::UInt32:
    case DataType::Float32: {
        const quint32 bits = mergeWords(values[offset], values[offset + 1], tag.byteOrder);
        if (tag.dataType == DataType::Int32)
            result.value = toEngineering(QVariant(int(qint32(bits))), tag);
        else if (tag.dataType == DataType::UInt32)
            result.value = toEngineering(QVariant(quint32(bits)), tag);
        else {
            const float f = std::bit_cast<float>(bits);
            result.value = toEngineering(QVariant(double(f)), tag);
        }
        break;
    }
    case DataType::BitField: {
        const quint32 mask = tag.bitLength >= 32 ? 0xFFFFFFFFu : ((1u << tag.bitLength) - 1u);
        const quint32 raw = (values[offset] >> tag.bitPosition) & mask;
        result.value = QVariant(raw);
        break;
    }
    default:
        result.error = QStringLiteral("unknown data type %1").arg(static_cast<int>(tag.dataType));
        return result;
    }

    result.valid = true;
    return result;
}

QModbusDataUnit ValueCodec::encode(const Tag& tag, const QVariant& value)
{
    const QModbusDataUnit::RegisterType type = toModbusType(tag.registerType);

    if (!value.isValid()) {
        qWarning() << "ValueCodec::encode: invalid value for tag" << tag.name;
        return QModbusDataUnit(type, tag.address, 0);
    }

    QModbusDataUnit unit(type, tag.address, tag.registerCount());

    switch (tag.dataType) {
    case DataType::Bool: {
        const bool b = value.toBool();
        if (tag.registerType == RegisterType::Coil || tag.registerType == RegisterType::DiscreteInput)
            unit.setValue(0, b ? 1 : 0);
        else
            unit.setValue(0, b ? quint16(1u << tag.bitPosition) : 0);
        return unit;
    }
    case DataType::Int16: {
        const qint16 raw = static_cast<qint16>(std::lround(toRaw(value.toDouble(), tag)));
        unit.setValue(0, quint16(raw));
        return unit;
    }
    case DataType::UInt16: {
        const quint16 raw = static_cast<quint16>(std::lround(toRaw(value.toDouble(), tag)));
        unit.setValue(0, raw);
        return unit;
    }
    case DataType::Int32:
    case DataType::UInt32:
    case DataType::Float32: {
        quint32 bits = 0;
        if (tag.dataType == DataType::Int32)
            bits = static_cast<quint32>(
                static_cast<qint32>(std::lround(toRaw(value.toDouble(), tag))));
        else if (tag.dataType == DataType::UInt32)
            bits = static_cast<quint32>(std::lround(toRaw(value.toDouble(), tag)));
        else
            bits = std::bit_cast<quint32>(static_cast<float>(toRaw(value.toDouble(), tag)));

        const auto [word0, word1] = splitWords(bits, tag.byteOrder);
        unit.setValue(0, word0);
        unit.setValue(1, word1);
        return unit;
    }
    case DataType::BitField: {
        const quint32 mask = tag.bitLength >= 32 ? 0xFFFFFFFFu : ((1u << tag.bitLength) - 1u);
        const quint32 raw = (value.toUInt() & mask) << tag.bitPosition;
        unit.setValue(0, quint16(raw));
        return unit;
    }
    }

    qWarning() << "ValueCodec::encode: unknown data type" << static_cast<int>(tag.dataType)
               << "for tag" << tag.name;
    return QModbusDataUnit(type, tag.address, 0);
}

int ValueCodec::modbusAddress(const Tag& tag)
{
    return tag.address;
}

int ValueCodec::toBase0(int address, bool isOneBased)
{
    return isOneBased ? address - 1 : address;
}
