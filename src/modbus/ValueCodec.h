#pragma once

#include <QModbusDataUnit>
#include <QVariant>
#include <QString>

#include "domain/Tag.h"

// 解码结果：value 为工程值（已应用 scale/offset）
struct DecodeResult {
    QVariant value;
    bool valid = false;
    QString error;
};

Q_DECLARE_METATYPE(DecodeResult)

// 无状态工具类：Modbus 值编解码、字节序处理、地址转换（纯静态，任意线程可调）
class ValueCodec {
public:
    // 从读回复数据单元中按 tag 定义解码一个值
    static DecodeResult decode(const QModbusDataUnit& unit, const Tag& tag);
    // 将用户输入值按 tag 定义编码为写数据单元（起始地址 = tag 零基地址）
    static QModbusDataUnit encode(const Tag& tag, const QVariant& value);
    // 返回 tag 的零基 PDU 起始地址（恒等于 tag.address）
    static int modbusAddress(const Tag& tag);
    // UI 地址转换：isOneBased=true 时一基地址转零基（减 1），否则原样返回
    static int toBase0(int address, bool isOneBased);
};
