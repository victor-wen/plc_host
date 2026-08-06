#pragma once

#include <QString>
#include <cstdint>

enum class RegisterType : int {
    Coil = 0,
    DiscreteInput = 1,
    InputRegister = 2,
    HoldingRegister = 3
};

enum class DataType : int {
    Bool = 0,
    Int16 = 1,
    UInt16 = 2,
    Int32 = 3,
    UInt32 = 4,
    Float32 = 5,
    BitField = 6
};

enum class ByteOrder : int {
    ABCD = 0,
    DCBA = 1,
    BADC = 2,
    CDAB = 3
};

enum class WordOrder : int {
    HighLow = 0,
    LowHigh = 1
};

enum class HistoryMode : int {
    Periodic = 0,
    OnChange = 1
};

struct Tag {
    int id = -1;
    QString name;
    RegisterType registerType = RegisterType::HoldingRegister;
    int address = 0;
    DataType dataType = DataType::UInt16;
    ByteOrder byteOrder = ByteOrder::ABCD;
    WordOrder wordOrder = WordOrder::HighLow;
    int bitPosition = 0;
    int bitLength = 1;
    double scale = 1.0;
    double offset = 0.0;
    QString unit;
    bool readOnly = false;
    int pollGroup = 0;
    int pollIntervalMs = 500;
    bool historyEnabled = false;
    HistoryMode historyMode = HistoryMode::Periodic;

    int registerCount() const;
    int modbusAddress() const;
};
