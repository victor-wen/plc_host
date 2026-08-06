#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMutex>

#include "domain/TagValue.h"

// 线程安全值缓存（CORE-06）。
// 线程归属：跨线程共享 —— 通信线程写（AcquisitionEngine），UI 线程读。
// 非 QObject，内部以 QMutex 保护，全部公有方法互斥。
// 接口冻结于 docs/architecture/interfaces.md §6。
class TagCache {
public:
    explicit TagCache(int capacityHint = 1024);

    // 合并更新，不丢失未更新的 tag（按 tagId 覆盖/插入）。
    void updateValues(const QHash<int, TagValue>& values);

    // 全量拷贝（加锁）。返回后调用方与缓存无关。
    QHash<int, TagValue> snapshot() const;

    // 不存在返回默认 TagValue{tagId=-1, quality=Disconnected}。
    TagValue value(int tagId) const;

    // 返回 timestamp 早于 (now - thresholdMs) 的 tag id；
    // timestamp 无效（从未成功更新）也视为过期。
    QList<int> staleTagIds(int thresholdMs) const;

private:
    mutable QMutex m_mutex;
    QHash<int, TagValue> m_values;
};
