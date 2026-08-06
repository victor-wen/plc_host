#include "modbus/QtModbusClient.h"

#include <QVariant>

#include <QModbusTcpClient>

namespace {

QString errorStringFor(const QModbusDevice* device)
{
    if (device == nullptr)
        return QStringLiteral("Modbus device unavailable");
    return device->errorString();
}

} // namespace

QtModbusClient::QtModbusClient(QObject* parent)
    : IModbusClient(parent)
    , m_client(new QModbusTcpClient(this))
{
    connect(m_client, &QModbusDevice::stateChanged,
            this, [this](QModbusDevice::State state) {
        switch (state) {
        case QModbusDevice::ConnectedState:
            emit connected();
            break;
        case QModbusDevice::UnconnectedState:
            emit disconnected();
            break;
        default:
            break; // ConnectingState / ClosingState 不对外转发
        }
    });

    connect(m_client, &QModbusDevice::errorOccurred,
            this, [this](QModbusDevice::Error error) {
        Q_UNUSED(error);
        emit errorOccurred(errorStringFor(m_client));
    });
}

QtModbusClient::~QtModbusClient() = default;

void QtModbusClient::connectToDevice(const QString& host, int port, int unitId)
{
    m_unitId = unitId;

    // 连接参数变更必须断开后重新连接（见 interfaces.md §2）
    if (m_client->state() != QModbusDevice::UnconnectedState)
        m_client->disconnectDevice();

    m_client->setConnectionParameter(QModbusDevice::NetworkAddressParameter, host);
    m_client->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);

    if (!m_client->connectDevice())
        emit errorOccurred(errorStringFor(m_client));
}

void QtModbusClient::disconnectFromDevice()
{
    if (m_client->state() != QModbusDevice::UnconnectedState)
        m_client->disconnectDevice();
}

bool QtModbusClient::isConnected() const
{
    return m_client->state() == QModbusDevice::ConnectedState;
}

QModbusReply* QtModbusClient::sendReadRequest(const QModbusDataUnit& unit, int serverAddress)
{
    if (!isConnected())
        return nullptr; // 错误路径：未连接返回 nullptr，调用方判空
    return m_client->sendReadRequest(unit, serverAddress);
}

QModbusReply* QtModbusClient::sendWriteRequest(const QModbusDataUnit& unit, int serverAddress)
{
    if (!isConnected())
        return nullptr;
    return m_client->sendWriteRequest(unit, serverAddress);
}

void QtModbusClient::setTimeout(int ms)
{
    m_client->setTimeout(ms);
}

void QtModbusClient::setNumberOfRetries(int n)
{
    m_client->setNumberOfRetries(n);
}
