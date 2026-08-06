#pragma once

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVector>

#include "domain/Tag.h"
#include "domain/TagValue.h"
#include "modbus/IModbusClient.h"
#include "modbus/PollPlanner.h"
#include "runtime/WriteQueue.h"

class TagCache;

// 连接状态机（interfaces.md §8，冻结）。
// 转移：Disconnected → Connecting → Online ↔ Degraded → Reconnecting → Connecting → Online；
// 手动断开 → 直接 Disconnected（不进入 Reconnecting）。
enum class ConnectionState : int {
    Disconnected = 0,   // 未连接（初始态）/ 手动断开
    Connecting = 1,     // 正在建立连接/重连
    Online = 2,         // 在线且轮询正常
    Degraded = 3,       // 在线但有连续失败（质量 Bad，重试中）
    Reconnecting = 4    // 掉线后按退避自动重连中
};

Q_DECLARE_METATYPE(ConnectionState)
Q_DECLARE_METATYPE(TagValue)

// QHash<int, TagValue> 含逗号不能直接作宏实参，经别名声明元类型
// （底层类型与 QHash<int, TagValue> 相同，MetaTypeId 共享）。
using TagValueHash = QHash<int, TagValue>;
Q_DECLARE_METATYPE(TagValueHash)

// 采集引擎（CORE-08）。线程：通信线程（构造后 moveToThread 或直接在通信线程构造）。
// 职责：连接管理、按 PollGroup 周期轮询、质量降级（Stale/Bad）、重连退避、写队列调度。
// 引擎不拥有 IModbusClient；拥有 tags 拷贝与全部轮询定时器。
// 约束：任何时刻至多一个在途 Modbus 请求；写请求插在两次轮询间隙执行。
class AcquisitionEngine : public QObject {
    Q_OBJECT
public:
    explicit AcquisitionEngine(IModbusClient* client, TagCache* cache, QObject* parent = nullptr);
    ~AcquisitionEngine() override;

public slots:   // 均可经 QueuedConnection / invokeMethod 从其他线程调用
    void setTags(const QVector<Tag>& tags);
    void start(const QString& host, int port, int unitId);  // 建 PollGroup、开始轮询、发起连接
    void stop();                                            // 手动断开，不自动重连
    void enqueueWrite(const WriteCommand& cmd);
    void cancelPendingWrites();                             // 丢弃全部待写命令

public:
    ConnectionState state() const;

    // 重连退避基准间隔（默认 1000ms，序列 1/2/4/8/16/30 倍、上限 30s）。
    // 仅测试与调优时修改；不属于冻结接口。
    void setReconnectBackoffBase(int ms);

signals:
    void tagValuesUpdated(const QHash<int, TagValue>& snapshot);
    void connectionStateChanged(ConnectionState state);
    void writeCompleted(int tagId, bool success, const QString& error);

private:
    const Tag* findTag(int tagId) const;
    void buildPollGroups();
    void startPolling();
    void stopPolling();
    void onPollTimeout(int groupIndex);
    void sendReadForGroup(int groupIndex);
    void sendNextWrite();
    void onReplyFinished(QModbusReply* reply);
    void handleReadSuccess(int groupIndex, const QModbusDataUnit& unit);
    void handleReadFailure(int groupIndex, const QString& error);
    void updateConnectionHealth();
    void markAllTagsDisconnected();
    void abortPendingReply();
    void setConnectionState(ConnectionState state);
    void onClientConnected();
    void onClientDisconnected();
    void onClientError(const QString& message);
    void startReconnectTimer();
    void attemptReconnect();
    int nextBackoffMs() const;

    IModbusClient* m_client = nullptr;
    TagCache* m_cache = nullptr;

    QVector<Tag> m_tags;
    QVector<PollGroup> m_groups;
    QVector<QTimer*> m_pollTimers;
    QHash<int, int> m_groupFailures;   // groupIndex → 连续失败次数

    WriteQueue m_writeQueue;
    ConnectionState m_state = ConnectionState::Disconnected;

    QString m_host;
    int m_port = 502;
    int m_unitId = 1;

    QModbusReply* m_pendingReply = nullptr;  // 在途读/写回复；同一时刻至多一个
    int m_pendingGroup = -1;                 // 在途读请求的组索引
    int m_pendingWriteTagId = -1;            // 在途写请求的 tagId；-1 表示在途为读

    bool m_stopRequested = false;            // 手动断开标志：置位后不自动重连
    int m_reconnectAttempt = 0;              // 已进行的重连尝试次数（驱动退避序列）
    int m_reconnectBackoffBaseMs = 1000;
    QTimer* m_reconnectTimer = nullptr;
};
