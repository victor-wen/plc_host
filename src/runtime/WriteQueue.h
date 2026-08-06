#pragma once

#include <optional>

#include <QDateTime>
#include <QMutex>
#include <QUuid>
#include <QVariant>
#include <QVector>

// 写命令：由 UI 线程构造，经 QueuedConnection 转发到通信线程后入队。
struct WriteCommand {
    QUuid id;                     // 唯一标识（点动释放与按下通过同 id 关联）
    int tagId = -1;
    QVariant value;               // 目标值（工程值，编码在引擎内完成）
    QDateTime createdAt;
    int expiryMs = 5000;          // 存活时长，默认 5s
    bool isRelease = false;       // 点动释放标志
    int priority = 0;             // 0=普通, 1=高（点动释放强制置 1）
};

Q_DECLARE_METATYPE(WriteCommand)

// 线程安全写队列：AcquisitionEngine 在轮询间隙出队执行写请求。
// 出队顺序：优先级高者优先；同优先级按入队顺序 FIFO。
class WriteQueue {
public:
    void enqueue(const WriteCommand& cmd);
    void clear();                          // 丢弃全部待处理命令
    std::optional<WriteCommand> dequeue(); // 按优先级+FIFO 出队；空则 nullopt
    void removeExpired(qint64 nowMs);      // 丢弃 createdAt+expiryMs < nowMs 的命令
    bool isEmpty() const;

private:
    QVector<WriteCommand> m_queue;
    mutable QMutex m_mutex;
};
