#include <QTest>
#include <QModbusDataUnit>

#include "modbus/ValueCodec.h"

namespace {

QModbusDataUnit makeUnit(QModbusDataUnit::RegisterType type, int startAddress,
                         const QVector<quint16>& values)
{
    QModbusDataUnit unit(type, startAddress, 0);
    unit.setValues(values);
    return unit;
}

Tag makeTag(DataType dataType, ByteOrder byteOrder = ByteOrder::ABCD, int address = 0,
            RegisterType regType = RegisterType::HoldingRegister)
{
    Tag tag;
    tag.dataType = dataType;
    tag.byteOrder = byteOrder;
    tag.address = address;
    tag.registerType = regType;
    return tag;
}

} // namespace

class ValueCodecTest : public QObject {
    Q_OBJECT
private slots:
    void modbusAddress_zeroBased()
    {
        Tag tag;
        tag.address = 100;
        QCOMPARE(ValueCodec::modbusAddress(tag), 100);
        tag.address = 0;
        QCOMPARE(ValueCodec::modbusAddress(tag), 0);
    }

    void toBase0_zeroBased_unchanged()
    {
        QCOMPARE(ValueCodec::toBase0(0, false), 0);
        QCOMPARE(ValueCodec::toBase0(5, false), 5);
    }

    void toBase0_oneBased_subtractsOne()
    {
        QCOMPARE(ValueCodec::toBase0(1, true), 0);
        QCOMPARE(ValueCodec::toBase0(40001, true), 40000);
    }

    void decode_UInt16_ABCD()
    {
        Tag tag = makeTag(DataType::UInt16);
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0x1234});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toUInt(), 4660u);
    }

    void decode_UInt16_offsetWithinBlock()
    {
        Tag tag = makeTag(DataType::UInt16, ByteOrder::ABCD, 1);
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0x0001, 0x1234, 0xFFFF});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toUInt(), 4660u);
    }

    void decode_Int16_negative()
    {
        Tag tag = makeTag(DataType::Int16);
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0xFFFE});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toInt(), -2);
    }

    void decode_Int32_allByteOrders_positive()
    {
        const ByteOrder orders[4] = {
            ByteOrder::ABCD, ByteOrder::DCBA, ByteOrder::BADC, ByteOrder::CDAB
        };
        const QVector<quint16> regs[4] = {
            {0x1234, 0x5678}, // ABCD
            {0x5678, 0x1234}, // DCBA
            {0x3412, 0x7856}, // BADC
            {0x7856, 0x3412}  // CDAB
        };
        for (int i = 0; i < 4; ++i) {
            Tag tag = makeTag(DataType::Int32, orders[i]);
            auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, regs[i]);
            auto r = ValueCodec::decode(unit, tag);
            QVERIFY2(r.valid, qPrintable(r.error));
            QCOMPARE(r.value.toInt(), 0x12345678);
        }
    }

    void decode_Int32_allByteOrders_negative()
    {
        const ByteOrder orders[4] = {
            ByteOrder::ABCD, ByteOrder::DCBA, ByteOrder::BADC, ByteOrder::CDAB
        };
        const QVector<quint16> regs[4] = {
            {0xFFFF, 0xFFFE}, // ABCD
            {0xFFFE, 0xFFFF}, // DCBA
            {0xFFFF, 0xFEFF}, // BADC
            {0xFEFF, 0xFFFF}  // CDAB
        };
        for (int i = 0; i < 4; ++i) {
            Tag tag = makeTag(DataType::Int32, orders[i]);
            auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, regs[i]);
            auto r = ValueCodec::decode(unit, tag);
            QVERIFY2(r.valid, qPrintable(r.error));
            QCOMPARE(r.value.toInt(), -2);
        }
    }

    void decode_Float32_allByteOrders()
    {
        const ByteOrder orders[4] = {
            ByteOrder::ABCD, ByteOrder::DCBA, ByteOrder::BADC, ByteOrder::CDAB
        };
        const QVector<quint16> regs[4] = {
            {0x3F80, 0x0000}, // ABCD
            {0x0000, 0x3F80}, // DCBA
            {0x803F, 0x0000}, // BADC
            {0x0000, 0x803F}  // CDAB
        };
        for (int i = 0; i < 4; ++i) {
            Tag tag = makeTag(DataType::Float32, orders[i]);
            auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, regs[i]);
            auto r = ValueCodec::decode(unit, tag);
            QVERIFY2(r.valid, qPrintable(r.error));
            QCOMPARE(r.value.toFloat(), 1.0f);
        }
    }

    void decode_Float32_negative()
    {
        Tag tag = makeTag(DataType::Float32, ByteOrder::ABCD);
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0xC020, 0x0000});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toFloat(), -2.5f);
    }

    void decode_UInt32_max()
    {
        Tag tag = makeTag(DataType::UInt32, ByteOrder::ABCD);
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0xFFFF, 0xFFFF});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toUInt(), 0xFFFFFFFFu);
    }

    void decode_UInt32_allByteOrders()
    {
        const ByteOrder orders[4] = {
            ByteOrder::ABCD, ByteOrder::DCBA, ByteOrder::BADC, ByteOrder::CDAB
        };
        const QVector<quint16> regs[4] = {
            {0x1234, 0x5678}, // ABCD
            {0x5678, 0x1234}, // DCBA
            {0x3412, 0x7856}, // BADC
            {0x7856, 0x3412}  // CDAB
        };
        for (int i = 0; i < 4; ++i) {
            Tag tag = makeTag(DataType::UInt32, orders[i]);
            auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, regs[i]);
            auto r = ValueCodec::decode(unit, tag);
            QVERIFY2(r.valid, qPrintable(r.error));
            QCOMPARE(r.value.toUInt(), 0x12345678u);
        }
    }

    void decode_Bool_coil()
    {
        Tag tag = makeTag(DataType::Bool, ByteOrder::ABCD, 0, RegisterType::Coil);
        auto unit = makeUnit(QModbusDataUnit::Coils, 0, {1});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toBool(), true);

        unit = makeUnit(QModbusDataUnit::Coils, 0, {0});
        r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toBool(), false);
    }

    void decode_Bool_registerBit()
    {
        Tag tag = makeTag(DataType::Bool);
        tag.bitPosition = 3;
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0x0008});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toBool(), true);

        unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0x0000});
        r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toBool(), false);
    }

    void decode_BitField_singleBit()
    {
        Tag tag = makeTag(DataType::BitField);
        tag.bitPosition = 3;
        tag.bitLength = 1;
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0x0008});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toUInt(), 1u);

        unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0x0000});
        r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toUInt(), 0u);
    }

    void decode_BitField_multiBit()
    {
        Tag tag = makeTag(DataType::BitField);
        tag.bitPosition = 4;
        tag.bitLength = 4;
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0x00F0});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.value.toUInt(), 0xFu);
    }

    void encode_UInt16()
    {
        Tag tag = makeTag(DataType::UInt16, ByteOrder::ABCD, 100);
        auto unit = ValueCodec::encode(tag, QVariant(4660));
        QCOMPARE(unit.registerType(), QModbusDataUnit::HoldingRegisters);
        QCOMPARE(unit.startAddress(), 100);
        QCOMPARE(unit.valueCount(), 1);
        QCOMPARE(unit.values().at(0), quint16(0x1234));
    }

    void encode_Int32()
    {
        Tag tag = makeTag(DataType::Int32, ByteOrder::ABCD);
        auto unit = ValueCodec::encode(tag, QVariant(0x12345678));
        QCOMPARE(unit.valueCount(), 2);
        QCOMPARE(unit.values().at(0), quint16(0x1234));
        QCOMPARE(unit.values().at(1), quint16(0x5678));

        tag.byteOrder = ByteOrder::DCBA;
        unit = ValueCodec::encode(tag, QVariant(0x12345678));
        QCOMPARE(unit.values().at(0), quint16(0x5678));
        QCOMPARE(unit.values().at(1), quint16(0x1234));

        tag.byteOrder = ByteOrder::ABCD;
        unit = ValueCodec::encode(tag, QVariant(-1));
        QCOMPARE(unit.values().at(0), quint16(0xFFFF));
        QCOMPARE(unit.values().at(1), quint16(0xFFFF));
    }

    void encode_Float32_allByteOrders()
    {
        const ByteOrder orders[4] = {
            ByteOrder::ABCD, ByteOrder::DCBA, ByteOrder::BADC, ByteOrder::CDAB
        };
        const QVector<quint16> expected[4] = {
            {0x3F80, 0x0000}, // ABCD
            {0x0000, 0x3F80}, // DCBA
            {0x803F, 0x0000}, // BADC
            {0x0000, 0x803F}  // CDAB
        };
        for (int i = 0; i < 4; ++i) {
            Tag tag = makeTag(DataType::Float32, orders[i]);
            auto unit = ValueCodec::encode(tag, QVariant(1.0));
            QCOMPARE(unit.valueCount(), 2);
            QCOMPARE(unit.values().at(0), expected[i].at(0));
            QCOMPARE(unit.values().at(1), expected[i].at(1));
        }
    }

    void encode_Bool()
    {
        Tag tag = makeTag(DataType::Bool, ByteOrder::ABCD, 0, RegisterType::Coil);
        auto unit = ValueCodec::encode(tag, QVariant(true));
        QCOMPARE(unit.registerType(), QModbusDataUnit::Coils);
        QCOMPARE(unit.values().at(0), quint16(1));

        unit = ValueCodec::encode(tag, QVariant(false));
        QCOMPARE(unit.values().at(0), quint16(0));
    }

    void roundTrip_Int32_allByteOrders()
    {
        const ByteOrder orders[4] = {
            ByteOrder::ABCD, ByteOrder::DCBA, ByteOrder::BADC, ByteOrder::CDAB
        };
        for (ByteOrder order : orders) {
            Tag tag = makeTag(DataType::Int32, order);
            auto unit = ValueCodec::encode(tag, QVariant(-12345));
            auto r = ValueCodec::decode(unit, tag);
            QVERIFY2(r.valid, qPrintable(r.error));
            QCOMPARE(r.value.toInt(), -12345);
        }
    }

    void roundTrip_Float32_allByteOrders()
    {
        const ByteOrder orders[4] = {
            ByteOrder::ABCD, ByteOrder::DCBA, ByteOrder::BADC, ByteOrder::CDAB
        };
        for (ByteOrder order : orders) {
            Tag tag = makeTag(DataType::Float32, order);
            auto unit = ValueCodec::encode(tag, QVariant(1.5));
            auto r = ValueCodec::decode(unit, tag);
            QVERIFY2(r.valid, qPrintable(r.error));
            QCOMPARE(r.value.toFloat(), 1.5f);
        }
    }

    void decode_insufficientRegisterData_error()
    {
        Tag tag = makeTag(DataType::Int32); // needs 2 registers
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0x1234});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY(!r.valid);
        QVERIFY(!r.error.isEmpty());
    }

    void decode_addressOutOfRange_error()
    {
        Tag tag = makeTag(DataType::Int16, ByteOrder::ABCD, 10);
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {1, 2, 3, 4, 5});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY(!r.valid);
        QVERIFY(!r.error.isEmpty());
    }

    void decode_emptyUnit_error()
    {
        Tag tag = makeTag(DataType::UInt16);
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 0, 0);
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY(!r.valid);
        QVERIFY(!r.error.isEmpty());
    }

    void decode_invalidDataType_error()
    {
        Tag tag = makeTag(DataType::UInt16);
        tag.dataType = static_cast<DataType>(99);
        auto unit = makeUnit(QModbusDataUnit::HoldingRegisters, 0, {0x1234});
        auto r = ValueCodec::decode(unit, tag);
        QVERIFY(!r.valid);
        QVERIFY(!r.error.isEmpty());
    }
};

QTEST_MAIN(ValueCodecTest)
#include "tst_ValueCodec.moc"
