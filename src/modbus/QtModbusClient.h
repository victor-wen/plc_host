#pragma once

#include <QString>

#include "modbus/IModbusClient.h"

class QModbusTcpClient;

// IModbusClient 的 QModbusTcpClient 实现。
// 线程：通信线程。包装 QModbusTcpClient，转发状态/错误信号。
class QtModbusClient : public IModbusClient {
    Q_OBJECT
public:
    explicit QtModbusClient(QObject* parent = nullptr);
    ~QtModbusClient() override;

    void connectToDevice(const QString& host, int port, int unitId) override;
    void disconnectFromDevice() override;
    bool isConnected() const override;

    QModbusReply* sendReadRequest(const QModbusDataUnit& unit, int serverAddress) override;
    QModbusReply* sendWriteRequest(const QModbusDataUnit& unit, int serverAddress) override;

    void setTimeout(int ms) override;
    void setNumberOfRetries(int n) override;

private:
    QModbusTcpClient* m_client = nullptr;
    int m_unitId = 1;
};
