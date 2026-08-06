#include <QDateTime>
#include <QMap>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QSet>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QVector>

#include "modbus/IModbusClient.h"
#include "runtime/AcquisitionEngine.h"
#include "runtime/TagCache.h"

// ---------------------------------------------------------------------------
// FakeModbusClient：实现 IModbusClient，不连接真实网络。
// - 构造函数接收寄存器预置数据（QMap<RegisterType, QVector<quint16>>）。
// - sendReadRequest 返回按请求切片的预置值；sendWriteRequest 写回预置值。
// - 回复经 QTimer::singleShot(0, reply, ...) 异步完成，让引擎先建立 finished 连接；
//   context 为 reply 自身，reply 被 deleteLater 后回调自动取消，避免悬挂。
// - 注入方法：injectDisconnect / injectTimeout / injectException /
//   setReconnectFails（重连尝试持续失败，用于退避测试）。
// ---------------------------------------------------------------------------
class FakeModbusClient : public IModbusClient {
    Q_OBJECT
public:
    struct ConnectCall {
        qint64 timestampMs = 0;
        QString host;
        int port = 0;
        int unitId = 0;
    };

    explicit FakeModbusClient(
        const QMap<QModbusDataUnit::RegisterType, QVector<quint16>>& registers,
        QObject* parent = nullptr)
        : IModbusClient(parent)
        , m_registers(registers)
    {
    }

    void connectToDevice(const QString& host, int port, int unitId) override
    {
        m_connectCalls.append(
            ConnectCall{QDateTime::currentMSecsSinceEpoch(), host, port, unitId});
        m_host = host;
        m_port = port;
        m_unitId = unitId;
        if (m_reconnectFails && m_connectCalls.size() > 1) {
            m_connected = false;
            emit errorOccurred(QStringLiteral("simulated connect failure"));
            emit disconnected();
            return;
        }
        m_connected = true;
        emit connected();
    }

    void disconnectFromDevice() override
    {
        if (!m_connected)
            return;
        m_connected = false;
        emit disconnected();
    }

    bool isConnected() const override { return m_connected; }

    QModbusReply* sendReadRequest(const QModbusDataUnit& unit, int serverAddress) override
    {
        if (!m_connected)
            return nullptr;
        ++m_readRequests;
        auto* reply = new QModbusReply(QModbusReply::Common, serverAddress, this);
        QTimer::singleShot(0, reply, [this, reply, request = unit]() {
            if (m_timeoutPending) {
                m_timeoutPending = false;
                reply->setError(QModbusDevice::TimeoutError,
                                QStringLiteral("injected read timeout"));
            } else if (m_exceptionPending) {
                m_exceptionPending = false;
                reply->setError(QModbusDevice::ProtocolError,
                                QStringLiteral("injected protocol exception"));
            } else {
                reply->setResult(makeReadResult(request));
            }
            reply->setFinished(true);
            emit reply->finished();
        });
        return reply;
    }

    QModbusReply* sendWriteRequest(const QModbusDataUnit& unit, int serverAddress) override
    {
        if (!m_connected)
            return nullptr;
        ++m_writeRequests;
        applyWrite(unit);
        auto* reply = new QModbusReply(QModbusReply::Common, serverAddress, this);
        QTimer::singleShot(0, reply, [reply]() {
            reply->setFinished(true);
            emit reply->finished();
        });
        return reply;
    }

    void setTimeout(int ms) override { Q_UNUSED(ms) }
    void setNumberOfRetries(int n) override { Q_UNUSED(n) }

    // --- 测试注入 ---
    void injectDisconnect()
    {
        m_connected = false;
        emit disconnected();
    }

    // 下一次读回复以超时失败完成（一次性）
    void injectTimeout() { m_timeoutPending = true; }

    // 下一次读回复以协议异常完成（一次性）
    void injectException() { m_exceptionPending = true; }

    // 置位后所有重连尝试（connectToDevice 第 2 次起）失败
    void setReconnectFails(bool on) { m_reconnectFails = on; }

    // --- 观测 ---
    int connectCallCount() const { return m_connectCalls.size(); }
    qint64 connectCallTime(int index) const { return m_connectCalls.at(index).timestampMs; }
    int readRequestCount() const { return m_readRequests; }
    int writeRequestCount() const { return m_writeRequests; }

    quint16 registerValue(QModbusDataUnit::RegisterType type, int address) const
    {
        const auto it = m_registers.constFind(type);
        if (it == m_registers.constEnd() || address < 0 || address >= it->size())
            return 0;
        return it->at(address);
    }

private:
    QModbusDataUnit makeReadResult(const QModbusDataUnit& request) const
    {
        const auto it = m_registers.constFind(request.registerType());
        QVector<quint16> values;
        values.reserve(request.valueCount());
        if (it != m_registers.constEnd()) {
            for (int i = request.startAddress();
                 i < request.startAddress() + int(request.valueCount()); ++i) {
                values.append((i >= 0 && i < it->size()) ? it->at(i) : quint16(0));
            }
        } else {
            values.fill(0, int(request.valueCount()));
        }
        QModbusDataUnit result(request.registerType(), request.startAddress(), values);
        return result;
    }

    void applyWrite(const QModbusDataUnit& unit)
    {
        QVector<quint16>& regs = m_registers[unit.registerType()];
        const int needed = unit.startAddress() + int(unit.valueCount());
        if (regs.size() < needed)
            regs.resize(needed);
        for (int i = 0; i < int(unit.valueCount()); ++i)
            regs[unit.startAddress() + i] = unit.value(i);
    }

    QMap<QModbusDataUnit::RegisterType, QVector<quint16>> m_registers;
    QVector<ConnectCall> m_connectCalls;
    bool m_connected = false;
    bool m_timeoutPending = false;
    bool m_exceptionPending = false;
    bool m_reconnectFails = false;
    int m_readRequests = 0;
    int m_writeRequests = 0;
    QString m_host;
    int m_port = 502;
    int m_unitId = 1;
};

namespace {

Tag makeTag(int id, int address, DataType dataType = DataType::UInt16,
            int intervalMs = 50)
{
    Tag t;
    t.id = id;
    t.name = QStringLiteral("tag%1").arg(id);
    t.registerType = RegisterType::HoldingRegister;
    t.address = address;
    t.dataType = dataType;
    t.pollIntervalMs = intervalMs;
    return t;
}

// 扫描 QSignalSpy 历史中是否出现过含 Stale 质量的快照
// （单次超时后下一次轮询会恢复 Good，必须查历史而非只看当前值）。
bool hasStaleQuality(const QSignalSpy& spy)
{
    for (int i = 0; i < spy.count(); ++i) {
        const auto snap = spy.at(i).at(0).value<QHash<int, TagValue>>();
        for (auto it = snap.cbegin(); it != snap.cend(); ++it) {
            if (it.value().quality == Quality::Stale)
                return true;
        }
    }
    return false;
}

bool hasConnectionState(const QSignalSpy& spy, ConnectionState state)
{
    for (int i = 0; i < spy.count(); ++i) {
        if (spy.at(i).at(0).value<ConnectionState>() == state)
            return true;
    }
    return false;
}

} // namespace

// CORE-08 集成测试：FakeModbusClient + AcquisitionEngine + TagCache 全链路。
class AcquisitionEngineTest : public QObject {
    Q_OBJECT
private slots:
    void pollAndUpdate();
    void connectionStateSequence();
    void writeAndComplete();
    void reconnectBackoff();
    void staleOnFailure();
    void stop_doesNotAutoReconnect();
};

void AcquisitionEngineTest::pollAndUpdate()
{
    FakeModbusClient fake(
        {{QModbusDataUnit::HoldingRegisters, {quint16(100), quint16(200)}}});
    TagCache cache;
    AcquisitionEngine engine(&fake, &cache);
    engine.setTags({makeTag(1, 0), makeTag(2, 1)});

    QSignalSpy valueSpy(&engine, &AcquisitionEngine::tagValuesUpdated);
    engine.start("127.0.0.1", 502, 1);

    QVERIFY2(valueSpy.wait(2000), "no tagValuesUpdated after start");

    const auto snap = valueSpy.last().at(0).value<QHash<int, TagValue>>();
    QCOMPARE(snap.size(), 2);
    QVERIFY(snap.contains(1));
    QVERIFY(snap.contains(2));
    QCOMPARE(snap[1].value.toUInt(), 100u);
    QCOMPARE(snap[2].value.toUInt(), 200u);
    QCOMPARE(snap[1].quality, Quality::Good);
    QCOMPARE(snap[2].quality, Quality::Good);

    // TagCache 同步落库
    QCOMPARE(cache.value(1).value.toUInt(), 100u);
    QCOMPARE(cache.value(1).quality, Quality::Good);
    QCOMPARE(cache.value(2).value.toUInt(), 200u);

    engine.stop();
}

void AcquisitionEngineTest::connectionStateSequence()
{
    FakeModbusClient fake({{QModbusDataUnit::HoldingRegisters, {quint16(42)}}});
    TagCache cache;
    AcquisitionEngine engine(&fake, &cache);
    engine.setTags({makeTag(1, 0)});

    QCOMPARE(engine.state(), ConnectionState::Disconnected);

    QSignalSpy stateSpy(&engine, &AcquisitionEngine::connectionStateChanged);
    engine.start("127.0.0.1", 502, 1);

    // 初始连接序列：Connecting → Online
    QTRY_COMPARE(engine.state(), ConnectionState::Online);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy.at(0).at(0).value<ConnectionState>(), ConnectionState::Connecting);
    QCOMPARE(stateSpy.at(1).at(0).value<ConnectionState>(), ConnectionState::Online);

    engine.stop();
    // 手动断开 → 直接 Disconnected，不进入 Reconnecting
    QCOMPARE(engine.state(), ConnectionState::Disconnected);
}

void AcquisitionEngineTest::writeAndComplete()
{
    FakeModbusClient fake({{QModbusDataUnit::HoldingRegisters, {quint16(0)}}});
    TagCache cache;
    AcquisitionEngine engine(&fake, &cache);
    engine.setTags({makeTag(1, 0)});

    QSignalSpy writeSpy(&engine, &AcquisitionEngine::writeCompleted);
    engine.start("127.0.0.1", 502, 1);
    QTRY_COMPARE(engine.state(), ConnectionState::Online);

    WriteCommand cmd;
    cmd.tagId = 1;
    cmd.value = QVariant(1234);
    cmd.createdAt = QDateTime::currentDateTime();
    cmd.expiryMs = 5000;
    engine.enqueueWrite(cmd);

    QVERIFY2(writeSpy.wait(2000), "no writeCompleted after enqueueWrite");
    QCOMPARE(writeSpy.count(), 1);
    QCOMPARE(writeSpy.at(0).at(0).toInt(), 1);        // tagId
    QCOMPARE(writeSpy.at(0).at(1).toBool(), true);    // success
    QCOMPARE(writeSpy.at(0).at(2).toString(), QString());

    // 写入值到达"PLC"侧
    QCOMPARE(fake.registerValue(QModbusDataUnit::HoldingRegisters, 0), quint16(1234));
    QCOMPARE(fake.writeRequestCount(), 1);

    engine.stop();
}

void AcquisitionEngineTest::reconnectBackoff()
{
    FakeModbusClient fake({{QModbusDataUnit::HoldingRegisters, {quint16(7)}}});
    TagCache cache;
    AcquisitionEngine engine(&fake, &cache);
    engine.setTags({makeTag(1, 0)});
    engine.setReconnectBackoffBase(50);   // 加速测试：50/100/200/400/800/1500ms

    QSignalSpy stateSpy(&engine, &AcquisitionEngine::connectionStateChanged);
    engine.start("127.0.0.1", 502, 1);
    QTRY_COMPARE(engine.state(), ConnectionState::Online);
    QCOMPARE(fake.connectCallCount(), 1);

    // 掉线后重连全部失败，观测退避间隔递增
    fake.setReconnectFails(true);
    fake.injectDisconnect();

    QTRY_VERIFY_WITH_TIMEOUT(fake.connectCallCount() >= 4, 5000);

    const qint64 g1 = fake.connectCallTime(1) - fake.connectCallTime(0);   // ≈50ms
    const qint64 g2 = fake.connectCallTime(2) - fake.connectCallTime(1);   // ≈100ms
    const qint64 g3 = fake.connectCallTime(3) - fake.connectCallTime(2);   // ≈200ms

    QVERIFY2(g1 >= 40, qPrintable(QString("first backoff %1ms too short").arg(g1)));
    QVERIFY2(g2 > qint64(g1 * 1.2),
             qPrintable(QString("gap2 %1ms not > gap1 %2ms").arg(g2).arg(g1)));
    QVERIFY2(g3 > qint64(g2 * 1.2),
             qPrintable(QString("gap3 %1ms not > gap2 %2ms").arg(g3).arg(g2)));

    // 状态机经过 Reconnecting → Connecting
    QVERIFY(hasConnectionState(stateSpy, ConnectionState::Reconnecting));
    QVERIFY(hasConnectionState(stateSpy, ConnectionState::Connecting));

    engine.stop();
    QCOMPARE(engine.state(), ConnectionState::Disconnected);
}

void AcquisitionEngineTest::staleOnFailure()
{
    FakeModbusClient fake({{QModbusDataUnit::HoldingRegisters, {quint16(100)}}});
    TagCache cache;
    AcquisitionEngine engine(&fake, &cache);
    engine.setTags({makeTag(1, 0)});

    QSignalSpy valueSpy(&engine, &AcquisitionEngine::tagValuesUpdated);
    engine.start("127.0.0.1", 502, 1);

    QTRY_COMPARE_WITH_TIMEOUT(cache.value(1).quality, Quality::Good, 2000);
    QCOMPARE(cache.value(1).value.toUInt(), 100u);

    fake.injectTimeout();   // 注入单次读超时

    QTRY_VERIFY_WITH_TIMEOUT(hasStaleQuality(valueSpy), 2000);

    // 单次失败：保留旧值，质量降级为 Stale
    QCOMPARE(cache.value(1).quality, Quality::Stale);
    QCOMPARE(cache.value(1).value.toUInt(), 100u);

    // 下一次轮询成功后恢复 Good
    QTRY_COMPARE_WITH_TIMEOUT(cache.value(1).quality, Quality::Good, 2000);

    engine.stop();
}

void AcquisitionEngineTest::stop_doesNotAutoReconnect()
{
    FakeModbusClient fake({{QModbusDataUnit::HoldingRegisters, {quint16(1)}}});
    TagCache cache;
    AcquisitionEngine engine(&fake, &cache);
    engine.setTags({makeTag(1, 0)});
    engine.setReconnectBackoffBase(50);

    QSignalSpy stateSpy(&engine, &AcquisitionEngine::connectionStateChanged);
    engine.start("127.0.0.1", 502, 1);
    QTRY_COMPARE(engine.state(), ConnectionState::Online);
    QCOMPARE(fake.connectCallCount(), 1);

    engine.stop();   // 手动断开
    QCOMPARE(engine.state(), ConnectionState::Disconnected);

    // 等待一段时间，确认没有任何重连尝试
    QTest::qWait(250);
    QCOMPARE(fake.connectCallCount(), 1);
    QVERIFY(!hasConnectionState(stateSpy, ConnectionState::Reconnecting));
}

QTEST_MAIN(AcquisitionEngineTest)
#include "tst_AcquisitionEngine.moc"
