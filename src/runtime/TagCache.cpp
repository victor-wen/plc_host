#include "runtime/TagCache.h"

TagCache::TagCache(int capacityHint)
{
    if (capacityHint > 0)
        m_values.reserve(capacityHint);
}

void TagCache::updateValues(const QHash<int, TagValue>& values)
{
    QMutexLocker locker(&m_mutex);
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        m_values.insert(it.key(), it.value());
}

QHash<int, TagValue> TagCache::snapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_values;
}

TagValue TagCache::value(int tagId) const
{
    QMutexLocker locker(&m_mutex);
    return m_values.value(tagId);   // 缺失时返回默认构造（quality=Disconnected）
}

QList<int> TagCache::staleTagIds(int thresholdMs) const
{
    QMutexLocker locker(&m_mutex);
    const QDateTime now = QDateTime::currentDateTime();
    QList<int> stale;
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it) {
        const QDateTime& ts = it.value().timestamp;
        // 无效时间戳（从未成功更新）也视为过期
        if (!ts.isValid() || ts.msecsTo(now) > thresholdMs)
            stale.append(it.key());
    }
    return stale;
}
