#include "runtime/AcquisitionEngine.h"

#include <QDateTime>
#include <QModbusReply>
#include <QTimer>

#include <algorithm>
#include <iterator>

#include "modbus/ValueCodec.h"
#include "runtime/TagCache.h"

namespace {

// 重连退避序列倍数：1/2/4/8/16/30（interfaces.md §5）。
constexpr int kBackoffMultipliers[] = {1, 2, 4, 8, 16, 30};
constexpr int kMaxBackoffMs = 30000;
// 同一组连续失败达到该次数 → 质量 Bad 且连接状态 Degraded（interfaces.md §5）。
constexpr int kMaxGroupFailures = 3;

} // namespace

AcquisitionEngine::AcquisitionEngine(IModbusClient* client, TagCache* cache, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_cache(cache)
{
    // 跨队列信号参数注册（interfaces.md §0）。重复注册无害。
    qRegisterMetaType<ConnectionState>("ConnectionState");
    qRegisterMetaType<TagValue>("TagValue");
    qRegisterMetaType<TagValueHash>("QHash<int,TagValue>");
    qRegisterMetaType<WriteCommand>("WriteCommand");

    connect(m_client, &IModbusClient::connected, this, &AcquisitionEngine::onClientConnected);
    connect(m_client, &IModbusClient::disconnected, this, &AcquisitionEngine::onClientDisconnected);
    connect(m_client, &IModbusClient::errorOccurred, this, &AcquisitionEngine::onClientError);
}

AcquisitionEngine::~AcquisitionEngine()
{
    abortPendingReply();
}

void AcquisitionEngine::setTags(const QVector<Tag>& tags)
{
    m_tags = tags;
}

void AcquisitionEngine::start(const QString& host, int port, int unitId)
{
    m_host = host;
    m_port = port;
    m_unitId = unitId;
    m_stopRequested = false;
    m_reconnectAttempt = 0;
    m_groupFailures.clear();
    m_writeQueue.clear();
    if (m_reconnectTimer != nullptr)
        m_reconnectTimer->stop();

    abortPendingReply();
    stopPolling();
    buildPollGroups();

    setConnectionState(ConnectionState::Connecting);
    m_client->connectToDevice(host, port, unitId);
}

void AcquisitionEngine::stop()
{
    m_stopRequested = true;
    if (m_reconnectTimer != nullptr)
        m_reconnectTimer->stop();
    stopPolling();
    abortPendingReply();
    m_writeQueue.clear();
    setConnectionState(ConnectionState::Disconnected);
    // 手动断开：client 发出 disconnected 信号后 onClientDisconnected
    // 因 m_stopRequested=true 直接进入 Disconnected，不再自动重连。
    m_client->disconnectFromDevice();
}

void AcquisitionEngine::enqueueWrite(const WriteCommand& cmd)
{
    m_writeQueue.enqueue(cmd);
}

void AcquisitionEngine::cancelPendingWrites()
{
    m_writeQueue.clear();
}

ConnectionState AcquisitionEngine::state() const
{
    return m_state;
}

void AcquisitionEngine::setReconnectBackoffBase(int ms)
{
    m_reconnectBackoffBaseMs = std::max(1, ms);
}

const Tag* AcquisitionEngine::findTag(int tagId) const
{
    for (const Tag& tag : m_tags) {
        if (tag.id == tagId)
            return &tag;
    }
    return nullptr;
}

void AcquisitionEngine::buildPollGroups()
{
    PollPlanner planner;
    m_groups = planner.buildGroups(m_tags);
}

void AcquisitionEngine::startPolling()
{
    stopPolling();
    for (int i = 0; i < m_groups.size(); ++i) {
        auto* timer = new QTimer(this);
        timer->setTimerType(Qt::PreciseTimer);
        timer->setInterval(m_groups[i].intervalMs);
        connect(timer, &QTimer::timeout, this, [this, i]() { onPollTimeout(i); });
        timer->start();
        m_pollTimers.append(timer);
    }
    if (!m_groups.isEmpty())
        onPollTimeout(0);   // 连接建立后立即发起第一轮读取，不必等一个周期
}

void AcquisitionEngine::stopPolling()
{
    for (QTimer* timer : m_pollTimers) {
        timer->stop();
        timer->deleteLater();
    }
    m_pollTimers.clear();
}

void AcquisitionEngine::onPollTimeout(int groupIndex)
{
    if (m_pendingReply != nullptr)   // 在途请求上限 = 1
        return;
    if (m_state != ConnectionState::Online && m_state != ConnectionState::Degraded)
        return;
    if (!m_writeQueue.isEmpty()) {   // 写请求插在两次轮询间隙执行
        sendNextWrite();
        return;
    }
    sendReadForGroup(groupIndex);
}

void AcquisitionEngine::sendReadForGroup(int groupIndex)
{
    if (groupIndex < 0 || groupIndex >= m_groups.size())
        return;
    const PollGroup& group = m_groups[groupIndex];
    const QModbusDataUnit unit(group.registerType, group.startAddress, group.count);
    QModbusReply* reply = m_client->sendReadRequest(unit, m_unitId);
    if (reply == nullptr) {
        handleReadFailure(groupIndex, QStringLiteral("read request rejected (not connected)"));
        return;
    }
    m_pendingReply = reply;
    m_pendingGroup = groupIndex;
    m_pendingWriteTagId = -1;
    connect(reply, &QModbusReply::finished, this, [this, reply]() { onReplyFinished(reply); });
}

void AcquisitionEngine::sendNextWrite()
{
    m_writeQueue.removeExpired(QDateTime::currentMSecsSinceEpoch());
    const auto cmd = m_writeQueue.dequeue();
    if (!cmd)
        return;

    const Tag* tag = findTag(cmd->tagId);
    if (tag == nullptr || tag->readOnly) {
        emit writeCompleted(cmd->tagId, false, QStringLiteral("tag not found or read-only"));
        return;
    }

    const QModbusDataUnit unit = ValueCodec::encode(*tag, cmd->value);
    if (unit.valueCount() == 0) {   // 编码失败（非法值 / scale=0）
        emit writeCompleted(cmd->tagId, false, QStringLiteral("value encoding failed"));
        return;
    }

    QModbusReply* reply = m_client->sendWriteRequest(unit, m_unitId);
    if (reply == nullptr) {
        emit writeCompleted(cmd->tagId, false, QStringLiteral("write rejected (not connected)"));
        return;
    }
    m_pendingReply = reply;
    m_pendingGroup = -1;
    m_pendingWriteTagId = cmd->tagId;
    connect(reply, &QModbusReply::finished, this, [this, reply]() { onReplyFinished(reply); });
}

void AcquisitionEngine::onReplyFinished(QModbusReply* reply)
{
    if (reply != m_pendingReply) {
        reply->deleteLater();   // 已被 stop()/断开 aborted 的回复
        return;
    }
    m_pendingReply = nullptr;

    if (m_pendingWriteTagId >= 0) {
        const int tagId = m_pendingWriteTagId;
        m_pendingWriteTagId = -1;
        if (reply->error() == QModbusDevice::NoError)
            emit writeCompleted(tagId, true, QString());
        else
            emit writeCompleted(tagId, false, reply->errorString());
    } else {
        const int groupIndex = m_pendingGroup;
        m_pendingGroup = -1;
        if (reply->error() == QModbusDevice::NoError)
            handleReadSuccess(groupIndex, reply->result());
        else
            handleReadFailure(groupIndex, reply->errorString());
    }
    reply->deleteLater();
}

void AcquisitionEngine::handleReadSuccess(int groupIndex, const QModbusDataUnit& unit)
{
    if (groupIndex < 0 || groupIndex >= m_groups.size())
        return;
    const PollGroup& group = m_groups[groupIndex];
    m_groupFailures.remove(groupIndex);

    QHash<int, TagValue> snapshot;
    const QDateTime now = QDateTime::currentDateTime();
    for (int tagId : group.tagIds) {
        const Tag* tag = findTag(tagId);
        if (tag == nullptr)
            continue;
        const DecodeResult decoded = ValueCodec::decode(unit, *tag);
        TagValue tv;
        tv.tagId = tagId;
        tv.timestamp = now;
        if (decoded.valid) {
            tv.value = decoded.value;
            tv.quality = Quality::Good;
        } else {
            // 组内单 tag 解码失败（地址越界等）按单次失败处理，不视为网络故障
            tv.quality = Quality::Stale;
            tv.error = decoded.error;
        }
        snapshot.insert(tagId, tv);
    }
    if (snapshot.isEmpty())
        return;
    m_cache->updateValues(snapshot);
    emit tagValuesUpdated(snapshot);
    updateConnectionHealth();
}

void AcquisitionEngine::handleReadFailure(int groupIndex, const QString& error)
{
    if (groupIndex < 0 || groupIndex >= m_groups.size())
        return;
    const PollGroup& group = m_groups[groupIndex];
    const int failures = m_groupFailures.value(groupIndex, 0) + 1;
    m_groupFailures.insert(groupIndex, failures);

    QHash<int, TagValue> snapshot;
    const QDateTime now = QDateTime::currentDateTime();
    for (int tagId : group.tagIds) {
        TagValue tv = m_cache->value(tagId);   // 保留旧值，仅降级质量
        tv.tagId = tagId;
        tv.timestamp = now;
        tv.quality = (failures >= kMaxGroupFailures) ? Quality::Bad : Quality::Stale;
        tv.error = error;
        snapshot.insert(tagId, tv);
    }
    if (snapshot.isEmpty())
        return;
    m_cache->updateValues(snapshot);
    emit tagValuesUpdated(snapshot);
    updateConnectionHealth();
}

void AcquisitionEngine::updateConnectionHealth()
{
    bool anyBad = false;
    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groupFailures.value(i, 0) >= kMaxGroupFailures) {
            anyBad = true;
            break;
        }
    }
    if (m_state == ConnectionState::Online && anyBad)
        setConnectionState(ConnectionState::Degraded);
    else if (m_state == ConnectionState::Degraded && !anyBad)
        setConnectionState(ConnectionState::Online);
}

void AcquisitionEngine::markAllTagsDisconnected()
{
    if (m_tags.isEmpty())
        return;
    const auto cacheSnap = m_cache->snapshot();
    QHash<int, TagValue> snapshot;
    const QDateTime now = QDateTime::currentDateTime();
    for (const Tag& tag : m_tags) {
        TagValue tv = cacheSnap.value(tag.id);
        tv.tagId = tag.id;
        tv.timestamp = now;
        tv.quality = Quality::Disconnected;
        tv.error = QStringLiteral("connection lost");
        snapshot.insert(tag.id, tv);
    }
    m_cache->updateValues(snapshot);
    emit tagValuesUpdated(snapshot);
}

void AcquisitionEngine::abortPendingReply()
{
    if (m_pendingReply == nullptr)
        return;
    m_pendingReply->disconnect(this);   // 断开引擎侧的 finished 连接，避免重复处理
    m_pendingReply->deleteLater();
    m_pendingReply = nullptr;
    m_pendingGroup = -1;
    m_pendingWriteTagId = -1;
}

void AcquisitionEngine::setConnectionState(ConnectionState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit connectionStateChanged(state);
}

void AcquisitionEngine::onClientConnected()
{
    m_stopRequested = false;
    m_reconnectAttempt = 0;
    m_groupFailures.clear();
    if (m_reconnectTimer != nullptr)
        m_reconnectTimer->stop();
    setConnectionState(ConnectionState::Online);
    startPolling();
}

void AcquisitionEngine::onClientDisconnected()
{
    abortPendingReply();
    stopPolling();
    markAllTagsDisconnected();

    if (m_stopRequested) {
        setConnectionState(ConnectionState::Disconnected);
        return;
    }
    // 掉线 → 自动重连退避（手动断开在 stop() 中置位 m_stopRequested，不会进入此分支）
    setConnectionState(ConnectionState::Reconnecting);
    startReconnectTimer();
}

void AcquisitionEngine::onClientError(const QString& message)
{
    Q_UNUSED(message)
    if (m_stopRequested)
        return;
    // 连接尝试失败通常伴随 disconnected（状态回 Unconnected），重连以 disconnected 为主路径。
    // 此处兜底：若仅 errorOccurred 且仍处于 Connecting/Reconnecting，则进入退避重连。
    if (m_state == ConnectionState::Connecting || m_state == ConnectionState::Reconnecting) {
        abortPendingReply();
        stopPolling();
        setConnectionState(ConnectionState::Reconnecting);
        startReconnectTimer();
    }
}

void AcquisitionEngine::startReconnectTimer()
{
    if (m_reconnectTimer == nullptr) {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setSingleShot(true);
        m_reconnectTimer->setTimerType(Qt::PreciseTimer);
        connect(m_reconnectTimer, &QTimer::timeout, this, &AcquisitionEngine::attemptReconnect);
    }
    m_reconnectTimer->start(nextBackoffMs());
}

void AcquisitionEngine::attemptReconnect()
{
    if (m_stopRequested)
        return;
    ++m_reconnectAttempt;
    setConnectionState(ConnectionState::Connecting);
    m_client->connectToDevice(m_host, m_port, m_unitId);
}

int AcquisitionEngine::nextBackoffMs() const
{
    const int idx = std::min(m_reconnectAttempt,
                             int(std::size(kBackoffMultipliers)) - 1);
    return std::min(m_reconnectBackoffBaseMs * kBackoffMultipliers[idx], kMaxBackoffMs);
}
