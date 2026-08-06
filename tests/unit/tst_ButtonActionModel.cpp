#include <QJsonObject>
#include <QTest>

#include "dashboard/runtime/ButtonAction.h"

// ButtonAction 模型测试 (Task DASH-07)：五种动作类型的 toJson/fromJson
// 往返、默认值、无效 JSON 失败路径。
class ButtonActionModelTest : public QObject {
    Q_OBJECT
private slots:
    void defaultAction_isFixedValue()
    {
        ButtonAction action;
        QCOMPARE(action.type, ButtonActionType::FixedValue);
        QVERIFY(!action.paramA.isValid());
        QVERIFY(!action.paramB.isValid());
        QCOMPARE(action.targetPageId, -1);
        QVERIFY(action.confirmMessage.isEmpty());
    }

    void momentary_pressReleaseValues_roundtrip()
    {
        ButtonAction action;
        action.type = ButtonActionType::Momentary;
        action.paramA = 1;
        action.paramB = 0;

        bool ok = false;
        const ButtonAction restored = ButtonAction::fromJson(action.toJson(), &ok);
        QVERIFY(ok);
        QCOMPARE(restored.type, ButtonActionType::Momentary);
        QCOMPARE(restored.paramA.toInt(), 1);
        QCOMPARE(restored.paramB.toInt(), 0);
        QCOMPARE(restored.targetPageId, -1);
        // 序列化幂等：反序列化后再序列化与原始 JSON 一致。
        QCOMPARE(action.toJson(), restored.toJson());
    }

    void toggle_onOffValues_roundtrip()
    {
        ButtonAction action;
        action.type = ButtonActionType::Toggle;
        action.paramA = true;
        action.paramB = false;

        bool ok = false;
        const ButtonAction restored = ButtonAction::fromJson(action.toJson(), &ok);
        QVERIFY(ok);
        QCOMPARE(restored.type, ButtonActionType::Toggle);
        QCOMPARE(restored.paramA.toBool(), true);
        QCOMPARE(restored.paramB.toBool(), false);
    }

    void fixedValue_targetValue_roundtrip()
    {
        ButtonAction action;
        action.type = ButtonActionType::FixedValue;
        action.paramA = 42;

        bool ok = false;
        const ButtonAction restored = ButtonAction::fromJson(action.toJson(), &ok);
        QVERIFY(ok);
        QCOMPARE(restored.type, ButtonActionType::FixedValue);
        QCOMPARE(restored.paramA.toInt(), 42);
        QVERIFY(!restored.paramB.isValid());
    }

    void inputValue_withConfirmMessage_roundtrip()
    {
        ButtonAction action;
        action.type = ButtonActionType::InputValue;
        action.confirmMessage = QStringLiteral("确定写入该值？");

        bool ok = false;
        const ButtonAction restored = ButtonAction::fromJson(action.toJson(), &ok);
        QVERIFY(ok);
        QCOMPARE(restored.type, ButtonActionType::InputValue);
        QCOMPARE(restored.confirmMessage, QStringLiteral("确定写入该值？"));
    }

    void navigatePage_targetPage_roundtrip()
    {
        ButtonAction action;
        action.type = ButtonActionType::NavigatePage;
        action.targetPageId = 2;

        bool ok = false;
        const ButtonAction restored = ButtonAction::fromJson(action.toJson(), &ok);
        QVERIFY(ok);
        QCOMPARE(restored.type, ButtonActionType::NavigatePage);
        QCOMPARE(restored.targetPageId, 2);
    }

    void json_usesReadableTypeNames()
    {
        ButtonAction momentary;
        momentary.type = ButtonActionType::Momentary;
        QCOMPARE(momentary.toJson().value(QStringLiteral("type")).toString(),
                 QStringLiteral("momentary"));

        ButtonAction navigate;
        navigate.type = ButtonActionType::NavigatePage;
        QCOMPARE(navigate.toJson().value(QStringLiteral("type")).toString(),
                 QStringLiteral("navigatePage"));
    }

    void invalidJson_parsingFails()
    {
        // 空对象：type 缺失 → 失败，返回默认动作。
        bool ok = true;
        const ButtonAction missing = ButtonAction::fromJson(QJsonObject(), &ok);
        QVERIFY(!ok);
        QCOMPARE(missing.type, ButtonActionType::FixedValue);

        // 未知 type 字符串 → 失败。
        QJsonObject unknown;
        unknown.insert(QStringLiteral("type"), QStringLiteral("teleport"));
        ok = true;
        const ButtonAction badType = ButtonAction::fromJson(unknown, &ok);
        QVERIFY(!ok);

        // type 非字符串 → 失败。
        QJsonObject nonString;
        nonString.insert(QStringLiteral("type"), 3.14);
        ok = true;
        const ButtonAction nonStr = ButtonAction::fromJson(nonString, &ok);
        QVERIFY(!ok);
    }
};

QTEST_MAIN(ButtonActionModelTest)
#include "tst_ButtonActionModel.moc"
