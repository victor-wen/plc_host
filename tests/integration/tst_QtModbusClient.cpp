#include <QModbusDataUnit>
#include <QModbusReply>
#include <QModbusTcpServer>
#include <QSignalSpy>
#include <QTest>

#include "modbus/IModbusClient.h"
#include "modbus/QtModbusClient.h"

// 集成测试：使用 QModbusTcpServer 在 localhost 上模拟 PLC。
// 覆盖：连接/断开、信号转发、读保持寄存器、写保持寄存器。
class QtModbusClientTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void connectAndDisconnect();
    void connectSignals();
    void readHoldingRegisters();
    void writeHoldingRegister();
    void requestWhileDisconnected_returnsNullptr();
    void cleanupTestCase();

private:
    void startServer();
    QModbusTcpServer* m_server = nullptr;
    int m_port = 15502;
};

void QtModbusClientTest::initTestCase()
{
    m_server = new QModbusTcpServer(this);
    m_server->setConnectionParameter(QModbusDevice::NetworkAddressParameter, "127.0.0.1");
    m_server->setConnectionParameter(QModbusDevice::NetworkPortParameter, m_port);
    // QModbusTcpServer 默认 serverAddress=255；客户端请求 unitId=1，
    // 不匹配的 unit id 会被服务器静默丢弃（qmodbustcpserver_p.cpp matchingServerAddress）
    m_server->setServerAddress(1);

    QModbusDataUnitMap map;
    map.insert(QModbusDataUnit::HoldingRegisters,
               QModbusDataUnit(QModbusDataUnit::HoldingRegisters, 0, 10));
    QVERIFY2(m_server->setMap(map), "server map setup failed");

    // QModbusTcpServer::connectDevice() 是同步的：状态在调用期间即变为
    // ConnectedState（若监听失败则为 UnconnectedState），不能依赖异步 wait()
    QVERIFY2(m_server->connectDevice(), "server connectDevice() failed");
    QVERIFY2(m_server->state() == QModbusDevice::ConnectedState,
             qPrintable(m_server->errorString()));
}

void QtModbusClientTest::cleanupTestCase()
{
    if (m_server)
        m_server->disconnectDevice();
    delete m_server;
    m_server = nullptr;
}

void QtModbusClientTest::connectAndDisconnect()
{
    QtModbusClient client;
    client.setTimeout(1000);
    client.setNumberOfRetries(0);

    QSignalSpy connectedSpy(&client, &IModbusClient::connected);
    client.connectToDevice("127.0.0.1", m_port, 1);
    QVERIFY(connectedSpy.wait(3000));
    QVERIFY(client.isConnected());

    QSignalSpy disconnectedSpy(&client, &IModbusClient::disconnected);
    client.disconnectFromDevice();
    // QModbusTcpClient 断开时同步发出状态信号，wait() 捕获不到已同步发生的信号
    QVERIFY(disconnectedSpy.count() > 0);
    QVERIFY(!client.isConnected());
}

void QtModbusClientTest::connectSignals()
{
    QtModbusClient client;
    client.setTimeout(1000);
    client.setNumberOfRetries(0);

    QSignalSpy connectedSpy(&client, &IModbusClient::connected);
    client.connectToDevice("127.0.0.1", m_port, 1);
    QVERIFY(connectedSpy.wait(3000));
    QCOMPARE(connectedSpy.count(), 1);

    QSignalSpy disconnectedSpy(&client, &IModbusClient::disconnected);
    client.disconnectFromDevice();
    QVERIFY(disconnectedSpy.count() > 0);
    QCOMPARE(disconnectedSpy.count(), 1);
}

void QtModbusClientTest::readHoldingRegisters()
{
    QtModbusClient client;
    client.setTimeout(1000);
    client.setNumberOfRetries(0);

    QSignalSpy connectedSpy(&client, &IModbusClient::connected);
    client.connectToDevice("127.0.0.1", m_port, 1);
    QVERIFY(connectedSpy.wait(3000));

    // 服务器侧写入期望值，再通过客户端读回
    QVERIFY(m_server->setData(QModbusDataUnit::HoldingRegisters, 3, 42));
    QVERIFY(m_server->setData(QModbusDataUnit::HoldingRegisters, 4, 0xCAFE));

    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 3, 2);
    QModbusReply* reply = client.sendReadRequest(readUnit, 1);
    QVERIFY(reply != nullptr);
    QVERIFY(!reply->isFinished());

    QSignalSpy finishedSpy(reply, &QModbusReply::finished);
    QVERIFY(finishedSpy.wait(3000));
    QCOMPARE(reply->error(), QModbusDevice::NoError);

    const QModbusDataUnit result = reply->result();
    QCOMPARE(result.registerType(), QModbusDataUnit::HoldingRegisters);
    QCOMPARE(result.startAddress(), 3);
    QCOMPARE(result.valueCount(), 2);
    QCOMPARE(result.value(0), quint16(42));
    QCOMPARE(result.value(1), quint16(0xCAFE));

    reply->deleteLater();
    client.disconnectFromDevice();
}

void QtModbusClientTest::writeHoldingRegister()
{
    QtModbusClient client;
    client.setTimeout(1000);
    client.setNumberOfRetries(0);

    QSignalSpy connectedSpy(&client, &IModbusClient::connected);
    client.connectToDevice("127.0.0.1", m_port, 1);
    QVERIFY(connectedSpy.wait(3000));

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 5, 1);
    writeUnit.setValues({quint16(1234)});
    QModbusReply* reply = client.sendWriteRequest(writeUnit, 1);
    QVERIFY(reply != nullptr);

    QSignalSpy finishedSpy(reply, &QModbusReply::finished);
    QVERIFY(finishedSpy.wait(3000));
    QCOMPARE(reply->error(), QModbusDevice::NoError);

    // 服务器回读验证写入值
    quint16 written = 0;
    QVERIFY(m_server->data(QModbusDataUnit::HoldingRegisters, 5, &written));
    QCOMPARE(written, quint16(1234));

    reply->deleteLater();
    client.disconnectFromDevice();
}

void QtModbusClientTest::requestWhileDisconnected_returnsNullptr()
{
    QtModbusClient client;
    client.setTimeout(1000);
    client.setNumberOfRetries(0);

    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 0, 1);
    QVERIFY(client.sendReadRequest(readUnit, 1) == nullptr);

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 0, 1);
    writeUnit.setValues({quint16(1)});
    QVERIFY(client.sendWriteRequest(writeUnit, 1) == nullptr);
}

QTEST_MAIN(QtModbusClientTest)
#include "tst_QtModbusClient.moc"
