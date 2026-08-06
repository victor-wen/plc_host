#include "Tag.h"

int Tag::registerCount() const
{
    switch (dataType) {
    case DataType::Bool:
        return registerType == RegisterType::Coil ? 1 : 1;
    case DataType::Int16:
    case DataType::UInt16:
        return 1;
    case DataType::Int32:
    case DataType::UInt32:
    case DataType::Float32:
        return 2;
    case DataType::BitField:
        return 1;
    }
    return 1;
}

int Tag::modbusAddress() const
{
    return address;
}
