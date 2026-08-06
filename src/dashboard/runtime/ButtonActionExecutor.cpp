#include "dashboard/runtime/ButtonActionExecutor.h"

#include <QDateTime>
#include <QMessageBox>
#include <QTimer>

#include "runtime/AcquisitionEngine.h"   // ConnectionState 完整定义（interfaces.md §8）
#include "runtime/TagCache.h"

namespace {

// QVariant 跨类型相等比较：同类型直接 ==；
// 否则按 bool → 数值 → 字符串 归一化比较，覆盖 线圈场景（缓存 bool true vs 配置 int 1）
// 的类型不匹配。
bool sameValue(const QVariant& lhs, const QVariant& rhs)
{
    if (lhs.userType() == rhs.userType())
        return lhs == rhs;
    if (lhs.userType() == QMetaType::Bool || rhs.userType() == QMetaType::Bool)
        return lhs.toBool() == rhs.toBool();
    if (lhs.canConvert<double>() && rhs.canConvert<double>())
        return lhs.toDouble() == rhs.toDouble();
    return lhs.toString() == rhs.toString();
}

// 确认弹窗（interfaces.md §11）。confirmMessage 非空时在 UI 线程同步弹出，
// 用户点"否"则中止执行。返回 true 表示继续。
bool confirmOrAbort(const QString& message)
{
    if (message.isEmpty())
        return true;
    return QMessageBox::question(nullptr, QStringLiteral("确认操作"), message,
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
           == QMessageBox::Yes;
}

} // namespace

ButtonActionExecutor::ButtonActionExecutor(QObject* parent)
    : QObject(parent)
{
}

void ButtonActionExecutor::setTagCache(TagCache* cache)
{
    m_cache = cache;
}

void ButtonActionExecutor::setTags(const QList<Tag>& tags)
{
    m_tags = tags;
}

void ButtonActionExecutor::setConnectionState(ConnectionState state)
{
    m_connState = state;
}

bool ButtonActionExecutor::isTagReadOnly(int tagId) const
{
    for (const Tag& tag : m_tags) {
        if (tag.id == tagId)
            return tag.readOnly;
    }
    // 未配置 Tag 定义：不拦截（由连接/质量检查兜底）。
    return false;
}

bool ButtonActionExecutor::isButtonEnabled(int tagId) const
{
    // 1. 连接状态：离线或重连退避中不可用。
    if (m_connState == ConnectionState::Disconnected
        || m_connState == ConnectionState::Reconnecting)
        return false;
    // 2. Tag 只读不可写。
    if (isTagReadOnly(tagId))
        return false;
    // 3. 质量：Bad/Disconnected 不可用。
    const TagValue v = m_cache ? m_cache->value(tagId) : TagValue{};
    if (v.quality == Quality::Bad || v.quality == Quality::Disconnected)
        return false;
    // 4. 值过期：超过 timeout×2 ms 未更新视为过期（timestamp 无效 = 从未成功读）。
    if (!v.timestamp.isValid()
        || v.timestamp.msecsTo(QDateTime::currentDateTime()) > kValueStaleThresholdMs)
        return false;
    return true;
}

void ButtonActionExecutor::execute(const ButtonAction& action, int tagId)
{
    // NavigatePage 不写值、不依赖 tag：离线/只读/无值都不应拦截页面跳转。
    if (action.type != ButtonActionType::NavigatePage) {
        if (!isButtonEnabled(tagId)) {
            emit actionRejected(tagId,
                                QStringLiteral("按钮不可用：离线、只读、质量异常或值过期"));
            return;
        }
        // 写类型必须配置有效参数。
        if ((action.type == ButtonActionType::Momentary
             || action.type == ButtonActionType::FixedValue)
            && !action.paramA.isValid()) {
            emit actionRejected(tagId, QStringLiteral("动作参数未配置（paramA 无效）"));
            return;
        }
        if (action.type == ButtonActionType::Toggle
            && (!action.paramA.isValid() || !action.paramB.isValid())) {
            emit actionRejected(tagId, QStringLiteral("切换动作需配置 paramA/paramB"));
            return;
        }
    }

    if (!confirmOrAbort(action.confirmMessage))
        return;

    switch (action.type) {
    case ButtonActionType::Momentary: {
        WriteCommand press;
        press.tagId = tagId;
        press.value = action.paramA;
        press.isRelease = false;
        press.priority = 0;
        emit writeRequested(press);

        MomentaryState state;
        state.pressId = press.id;
        // 松开值优先取 paramB；未配置则回退到按下前的缓存值。
        state.releaseValue = action.paramB.isValid()
            ? action.paramB
            : (m_cache ? m_cache->value(tagId).value : QVariant());
        m_momentary.insert(tagId, state);

        // 点动安全（interfaces.md §11）：3s 未手动释放则自动释放。
        QTimer::singleShot(kMomentaryHoldMs, this,
                           [this, tagId]() { releaseMomentary(tagId); });
        break;
    }
    case ButtonActionType::Toggle: {
        const QVariant current = m_cache ? m_cache->value(tagId).value : QVariant();
        WriteCommand cmd;
        cmd.tagId = tagId;
        cmd.value = sameValue(current, action.paramA) ? action.paramB : action.paramA;
        cmd.priority = 0;
        emit writeRequested(cmd);
        break;
    }
    case ButtonActionType::FixedValue: {
        WriteCommand cmd;
        cmd.tagId = tagId;
        cmd.value = action.paramA;
        cmd.priority = 0;
        emit writeRequested(cmd);
        break;
    }
    case ButtonActionType::InputValue:
        // 简化路径：通知 UI 弹出输入框并校验；确认后由 UI 层携结果发起写。
        emit inputRequested(tagId, action);
        break;
    case ButtonActionType::NavigatePage:
        emit pageNavigationRequested(action.targetPageId);
        break;
    }
}

void ButtonActionExecutor::releaseMomentary(int tagId)
{
    const auto it = m_momentary.find(tagId);
    if (it == m_momentary.end())
        return;   // 未按下（已释放或超时处理过）→ 幂等空操作

    WriteCommand release;
    release.tagId = tagId;
    release.value = it->releaseValue;
    release.id = it->pressId;       // 释放与按下同 id 关联（interfaces.md §7）
    release.isRelease = true;
    release.priority = 1;           // 点动释放强制高优先级
    m_momentary.erase(it);
    emit writeRequested(release);
}

void ButtonActionExecutor::releaseAllMomentary()
{
    const QList<int> active = m_momentary.keys();
    for (int tagId : active)
        releaseMomentary(tagId);
}
