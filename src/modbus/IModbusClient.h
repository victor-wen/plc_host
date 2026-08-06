#pragma once

#include <QObject>
#include <QModbusDataUnit>
#include <QModbusReply>

// Modbus 客户端抽象基类（冻结接口，见 docs/architecture/interfaces.md §2）。
// 线程：通信线程。对象创建于通信线程，不得跨线程直接调用。
// 实现：QtModbusClient（包装 QModbusTcpClient），可替换为 libmodbus 而不影响上层。
class IModbusClient : public QObject {
    Q_OBJECT
public:
    explicit IModbusClient(QObject* parent = nullptr)
        : QObject(parent) {}
    virtual ~IModbusClient() = default;

    // 建立连接：host/port/unitId。连接参数变更必须断开后重新连接。
    virtual void connectToDevice(const QString& host, int port, int unitId) = 0;
    // 主动断开。手动断开后 AcquisitionEngine 不得自动重连。
    virtual void disconnectFromDevice() = 0;
    virtual bool isConnected() const = 0;

    // 异步请求。返回的 QModbusReply 归调用方所有，必须 connect(finished) 后 deleteLater。
    // 同一时间最多一个在途请求（由 AcquisitionEngine 保证，本接口不强制）。
    // 错误路径：未连接时调用返回 nullptr；调用方必须判空。
    virtual QModbusReply* sendReadRequest(const QModbusDataUnit& unit, int serverAddress) = 0;
    virtual QModbusReply* sendWriteRequest(const QModbusDataUnit& unit, int serverAddress) = 0;

    virtual void setTimeout(int ms) = 0;
    virtual void setNumberOfRetries(int n) = 0;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
};
