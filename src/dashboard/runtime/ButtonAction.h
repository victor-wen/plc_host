#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QString>
#include <QVariant>

#include <optional>

// 按钮动作模型 (docs/architecture/interfaces.md §11, Task DASH-07)。
// 纯值类型：描述一次按钮触发应当产生的效果。ButtonItem 在运行模式点击
// 时发出 actionTriggered(action, tagId)，DASH-08 ButtonActionExecutor
// 负责实际执行（构造 WriteCommand / 页面跳转 / 确认弹窗）。
enum class ButtonActionType : int {
    Momentary = 0,   // 点动：按下写 paramA，松开写 paramB
    Toggle = 1,      // 切换：每次按下在 paramA(ON)/paramB(OFF) 之间翻转
    FixedValue = 2,  // 固定值：点击写 paramA
    InputValue = 3,  // 弹窗输入：输入校验后写入
    NavigatePage = 4 // 页面跳转：切换到 targetPageId
};

struct ButtonAction {
    ButtonActionType type = ButtonActionType::FixedValue;
    QVariant paramA;        // Momentary: 按下值 / FixedValue: 目标值 / Toggle: ON 值
    QVariant paramB;        // Momentary: 松开值 / Toggle: OFF 值
    int targetPageId = -1;  // NavigatePage 目标页
    QString confirmMessage; // 非空则先弹确认框

    // 序列化为 JSON。type 存字符串（人读友好、便于版本演进）；
    // paramA/paramB 未配置（invalid QVariant）时省略。
    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("type"), typeName());
        if (paramA.isValid())
            obj.insert(QStringLiteral("paramA"), QJsonValue::fromVariant(paramA));
        if (paramB.isValid())
            obj.insert(QStringLiteral("paramB"), QJsonValue::fromVariant(paramB));
        obj.insert(QStringLiteral("targetPageId"), targetPageId);
        obj.insert(QStringLiteral("confirmMessage"), confirmMessage);
        return obj;
    }

    // 反序列化。type 缺失或未知时失败：ok 置 false，返回默认动作。
    // 其余字段缺失时取各自默认值（容错）。
    static ButtonAction fromJson(const QJsonObject& obj, bool* ok = nullptr)
    {
        if (ok)
            *ok = false;
        ButtonAction action;
        const auto typeOpt = typeFromString(obj.value(QStringLiteral("type")).toString());
        if (!typeOpt.has_value())
            return action;
        if (ok)
            *ok = true;
        action.type = *typeOpt;
        action.paramA = obj.value(QStringLiteral("paramA")).toVariant();
        action.paramB = obj.value(QStringLiteral("paramB")).toVariant();
        action.targetPageId = obj.value(QStringLiteral("targetPageId")).toInt(-1);
        action.confirmMessage = obj.value(QStringLiteral("confirmMessage")).toString();
        return action;
    }

    // type 的字符串表示（JSON 持久化用）。
    QString typeName() const
    {
        switch (type) {
        case ButtonActionType::Momentary:
            return QStringLiteral("momentary");
        case ButtonActionType::Toggle:
            return QStringLiteral("toggle");
        case ButtonActionType::FixedValue:
            return QStringLiteral("fixedValue");
        case ButtonActionType::InputValue:
            return QStringLiteral("inputValue");
        case ButtonActionType::NavigatePage:
            return QStringLiteral("navigatePage");
        }
        return QStringLiteral("fixedValue");
    }

    // 字符串 → 枚举；未知字符串返回 nullopt。
    static std::optional<ButtonActionType> typeFromString(const QString& name)
    {
        if (name == QStringLiteral("momentary"))
            return ButtonActionType::Momentary;
        if (name == QStringLiteral("toggle"))
            return ButtonActionType::Toggle;
        if (name == QStringLiteral("fixedValue"))
            return ButtonActionType::FixedValue;
        if (name == QStringLiteral("inputValue"))
            return ButtonActionType::InputValue;
        if (name == QStringLiteral("navigatePage"))
            return ButtonActionType::NavigatePage;
        return std::nullopt;
    }
};

Q_DECLARE_METATYPE(ButtonActionType)
Q_DECLARE_METATYPE(ButtonAction)
