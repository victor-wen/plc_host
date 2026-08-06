#include "runtime/WriteQueue.h"

#include <QMutexLocker>

void WriteQueue::enqueue(const WriteCommand& cmd)
{
    QMutexLocker locker(&m_mutex);
    WriteCommand c = cmd;
    if (c.isRelease) {
        c.priority = 1;  // 点动释放强制高优先级
    }
    m_queue.append(c);
}

void WriteQueue::clear()
{
    QMutexLocker locker(&m_mutex);
    m_queue.clear();
}

std::optional<WriteCommand> WriteQueue::dequeue()
{
    QMutexLocker locker(&m_mutex);
    if (m_queue.isEmpty()) {
        return std::nullopt;
    }

    // 选择优先级最高的命令；同优先级取最早入队的（稳定取首个最大）。
    int best = 0;
    for (int i = 1; i < m_queue.size(); ++i) {
        if (m_queue[i].priority > m_queue[best].priority) {
            best = i;
        }
    }

    return m_queue.takeAt(best);
}

void WriteQueue::removeExpired(qint64 nowMs)
{
    QMutexLocker locker(&m_mutex);
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(nowMs);
    for (int i = m_queue.size() - 1; i >= 0; --i) {
        const WriteCommand& c = m_queue[i];
        // createdAt 无效时 msecsTo 返回 0，视为未过期，保守保留。
        if (c.createdAt.isValid() && c.createdAt.msecsTo(now) > c.expiryMs) {
            m_queue.removeAt(i);
        }
    }
}

bool WriteQueue::isEmpty() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue.isEmpty();
}
