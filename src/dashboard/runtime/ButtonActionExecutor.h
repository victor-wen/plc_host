#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QVariant>

#include "dashboard/runtime/ButtonAction.h"
#include "domain/Tag.h"
#include "runtime/WriteQueue.h"

class TagCache;

// ConnectionState 完整定义位于 runtime/AcquisitionEngine.h（interfaces.md §8）。
// 不透明枚举声明后类型即为完整类型，可作值成员/值参；枚举器在 .cpp 中可见。
enum class ConnectionState : int;

// 按钮动作执行器（DASH-08）。线程：UI 主线程。
// 职责：把 ButtonAction 翻译为 WriteCommand（经 writeRequested 转发到通信线程
// 引擎入队）或页面跳转；管理点动 按下/释放/3s 超时自动释放；统一按钮可用性判定。
//
// 与 docs/architecture/interfaces.md §11 的差异（本任务计划内的最小扩展）：
//  - 构造器改为无参 + setTagCache/setConnectionState/setTags（UI 装配期注入）；
//  - setTags 提供 isButtonEnabled 只读校验所需的 Tag 定义；
//  - inputRequested 信号让 UI 弹出输入对话框（InputValue 简化路径）；
//  - actionRejected 信号向上报告被拒绝的动作及原因。
class ButtonActionExecutor : public QObject {
    Q_OBJECT
public:
    explicit ButtonActionExecutor(QObject* parent = nullptr);

    void setTagCache(TagCache* cache);
    void setTags(const QList<Tag>& tags);
    void setConnectionState(ConnectionState state);

    void execute(const ButtonAction& action, int tagId);
    virtual void releaseMomentary(int tagId);   // virtual: 测试替身可覆写（DASH-09 SpyExecutor）
    virtual void releaseAllMomentary();
    bool isButtonEnabled(int tagId) const;

signals:
    void writeRequested(const WriteCommand& cmd);   // UI 层转发到引擎 enqueueWrite
    void pageNavigationRequested(int pageId);       // UI 层切换页面
    void actionRejected(int tagId, const QString& reason);
    void inputRequested(int tagId, const ButtonAction& action);  // UI 层弹输入框

private:
    static constexpr int kMomentaryHoldMs = 3000;      // 点动最大保持时长（超时自动释放）
    static constexpr int kValueStaleThresholdMs = 2 * kMomentaryHoldMs;  // 值过期阈值 = timeout×2

    bool isTagReadOnly(int tagId) const;

    TagCache* m_cache = nullptr;
    ConnectionState m_connState{};   // 值初始化 = 0 = Disconnected
    QList<Tag> m_tags;

    // 已按下未释放的点动按钮（tagId → 释放所需状态）。
    struct MomentaryState {
        QUuid pressId;          // 按下命令 id，释放命令复用（interfaces.md §7：同 id 关联）
        QVariant releaseValue;  // 松开时写入的值（paramB，或按下前的缓存值）
    };
    QHash<int, MomentaryState> m_momentary;
};
