#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include "dashboard/runtime/ButtonActionExecutor.h"
#include "domain/Tag.h"
#include "domain/TagValue.h"
#include "runtime/AcquisitionEngine.h"   // ConnectionState 完整定义
#include "runtime/TagCache.h"
#include "runtime/WriteQueue.h"

namespace {

Tag makeTag(int id, bool readOnly = false)
{
    Tag t;
    t.id = id;
    t.readOnly = readOnly;
    return t;
}

// 向缓存写入一个 tag 值（默认 Good + 当前时间戳）。
void seed(TagCache& cache, int tagId, const QVariant& value,
          Quality quality = Quality::Good,
          QDateTime timestamp = QDateTime::currentDateTime())
{
    TagValue v;
    v.tagId = tagId;
    v.value = value;
    v.quality = quality;
    v.timestamp = timestamp;
    QHash<int, TagValue> batch;
    batch.insert(tagId, v);
    cache.updateValues(batch);
}

void pressMomentary(ButtonActionExecutor& exec, int tagId,
                    const QVariant& pressValue, const QVariant& releaseValue)
{
    ButtonAction action;
    action.type = ButtonActionType::Momentary;
    action.paramA = pressValue;
    action.paramB = releaseValue;
    exec.execute(action, tagId);
}

// 测试夹具：TagCache + 已装配（在线、值 Good）的执行器。
struct Fixture {
    TagCache cache;
    ButtonActionExecutor exec;

    Fixture(int tagId = 1, const QVariant& value = QVariant(0),
            Quality quality = Quality::Good,
            QDateTime timestamp = QDateTime::currentDateTime())
    {
        exec.setTagCache(&cache);
        exec.setConnectionState(ConnectionState::Online);
        seed(cache, tagId, value, quality, timestamp);
    }
};

} // namespace

// ButtonActionExecutor 测试 (Task DASH-08)：五种动作类型的命令构造、
// 点动 按下/释放/3s 超时自动释放、isButtonEnabled 判定、页面跳转。
class ButtonActionExecutorTest : public QObject {
    Q_OBJECT
private slots:
    void momentaryPress_emitsWriteCommand_withPressSemantics()
    {
        Fixture f(1, QVariant(0));
        QSignalSpy writeSpy(&f.exec, &ButtonActionExecutor::writeRequested);

        pressMomentary(f.exec, 1, 1, 0);

        QCOMPARE(writeSpy.count(), 1);
        const WriteCommand cmd = writeSpy.at(0).at(0).value<WriteCommand>();
        QCOMPARE(cmd.tagId, 1);
        QCOMPARE(cmd.value.toInt(), 1);   // 按下写 paramA
        QVERIFY(!cmd.isRelease);
        QCOMPARE(cmd.priority, 0);

        f.exec.releaseAllMomentary();     // 清理点动状态，避免旧定时器干扰其他用例
    }

    void releaseMomentary_emitsReleaseCommand_sameIdAndHighPriority()
    {
        Fixture f(2, QVariant(0));
        QSignalSpy pressSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        pressMomentary(f.exec, 2, 1, 0);
        QCOMPARE(pressSpy.count(), 1);
        const WriteCommand pressCmd = pressSpy.at(0).at(0).value<WriteCommand>();

        QSignalSpy releaseSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        f.exec.releaseMomentary(2);

        QCOMPARE(releaseSpy.count(), 1);
        const WriteCommand releaseCmd = releaseSpy.at(0).at(0).value<WriteCommand>();
        QCOMPARE(releaseCmd.tagId, 2);
        QCOMPARE(releaseCmd.value.toInt(), 0);   // 松开写 paramB
        QVERIFY(releaseCmd.isRelease);
        QCOMPARE(releaseCmd.priority, 1);        // 点动释放强制高优先级
        QCOMPARE(releaseCmd.id, pressCmd.id);    // 按下/释放同 id 关联
    }

    void releaseMomentary_notPressed_emitsNothing()
    {
        Fixture f(2, QVariant(0));
        QSignalSpy writeSpy(&f.exec, &ButtonActionExecutor::writeRequested);

        f.exec.releaseMomentary(2);

        QCOMPARE(writeSpy.count(), 0);   // 幂等：未按下则空操作
    }

    void releaseMomentary_withoutParamB_usesPrePressCacheValue()
    {
        Fixture f(6, QVariant(5));   // 按下前缓存值 = 5
        ButtonAction action;
        action.type = ButtonActionType::Momentary;
        action.paramA = 1;           // paramB 未配置

        QSignalSpy pressSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        f.exec.execute(action, 6);
        QCOMPARE(pressSpy.count(), 1);

        QSignalSpy releaseSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        f.exec.releaseMomentary(6);
        QCOMPARE(releaseSpy.count(), 1);
        const WriteCommand cmd = releaseSpy.at(0).at(0).value<WriteCommand>();
        QVERIFY(cmd.isRelease);
        QCOMPARE(cmd.value.toInt(), 5);   // 回退为按下前缓存值
    }

    void releaseAllMomentary_releasesAllActiveButtons()
    {
        Fixture f(1, QVariant(0));
        seed(f.cache, 3, QVariant(0));
        seed(f.cache, 4, QVariant(0));

        QSignalSpy pressSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        pressMomentary(f.exec, 3, 1, 0);
        pressMomentary(f.exec, 4, 1, 0);
        QCOMPARE(pressSpy.count(), 2);

        QSignalSpy releaseSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        f.exec.releaseAllMomentary();

        QCOMPARE(releaseSpy.count(), 2);
        for (int i = 0; i < releaseSpy.count(); ++i) {
            const WriteCommand cmd = releaseSpy.at(i).at(0).value<WriteCommand>();
            QVERIFY(cmd.isRelease);
            QCOMPARE(cmd.priority, 1);
        }
    }

    void fixedValue_emitsWriteCommand_withTargetValue()
    {
        Fixture f(1, QVariant(0));
        QSignalSpy writeSpy(&f.exec, &ButtonActionExecutor::writeRequested);

        ButtonAction action;
        action.type = ButtonActionType::FixedValue;
        action.paramA = 42;
        f.exec.execute(action, 1);

        QCOMPARE(writeSpy.count(), 1);
        const WriteCommand cmd = writeSpy.at(0).at(0).value<WriteCommand>();
        QCOMPARE(cmd.tagId, 1);
        QCOMPARE(cmd.value.toInt(), 42);
        QVERIFY(!cmd.isRelease);
        QCOMPARE(cmd.priority, 0);
    }

    void toggle_currentEqualsParamA_writesParamB()
    {
        Fixture f(1, QVariant(1));   // 当前值 = 1 == paramA
        ButtonAction action;
        action.type = ButtonActionType::Toggle;
        action.paramA = 1;
        action.paramB = 0;

        QSignalSpy writeSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        f.exec.execute(action, 1);

        QCOMPARE(writeSpy.count(), 1);
        QCOMPARE(writeSpy.at(0).at(0).value<WriteCommand>().value.toInt(), 0);
    }

    void toggle_currentDiffersFromParamA_writesParamA()
    {
        Fixture f(1, QVariant(0));   // 当前值 = 0 != paramA
        ButtonAction action;
        action.type = ButtonActionType::Toggle;
        action.paramA = 1;
        action.paramB = 0;

        QSignalSpy writeSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        f.exec.execute(action, 1);

        QCOMPARE(writeSpy.count(), 1);
        QCOMPARE(writeSpy.at(0).at(0).value<WriteCommand>().value.toInt(), 1);
    }

    void toggle_boolCacheVsIntParam_comparesAcrossTypes()
    {
        Fixture f(1, QVariant(true));   // 缓存 bool true vs 配置 int 1 → 视为相同
        ButtonAction action;
        action.type = ButtonActionType::Toggle;
        action.paramA = 1;
        action.paramB = 0;

        QSignalSpy writeSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        f.exec.execute(action, 1);

        QCOMPARE(writeSpy.count(), 1);
        QCOMPARE(writeSpy.at(0).at(0).value<WriteCommand>().value.toInt(), 0);  // 写 paramB
    }

    void isButtonEnabled_offline_returnsFalse()
    {
        Fixture f(1, QVariant(0));

        f.exec.setConnectionState(ConnectionState::Disconnected);
        QVERIFY(!f.exec.isButtonEnabled(1));

        f.exec.setConnectionState(ConnectionState::Reconnecting);
        QVERIFY(!f.exec.isButtonEnabled(1));
    }

    void isButtonEnabled_onlineAndGood_returnsTrue()
    {
        Fixture f(1, QVariant(0));
        QVERIFY(f.exec.isButtonEnabled(1));
    }

    void isButtonEnabled_readOnly_returnsFalse()
    {
        Fixture f(1, QVariant(0));
        f.exec.setTags({makeTag(1, /*readOnly=*/true)});
        QVERIFY(!f.exec.isButtonEnabled(1));
    }

    void isButtonEnabled_badQuality_returnsFalse()
    {
        Fixture f(1, QVariant(0), Quality::Bad);
        QVERIFY(!f.exec.isButtonEnabled(1));

        Fixture disconnected(1, QVariant(0), Quality::Disconnected);
        QVERIFY(!disconnected.exec.isButtonEnabled(1));
    }

    void isButtonEnabled_staleValue_returnsFalse()
    {
        const QDateTime stale = QDateTime::currentDateTime().addMSecs(-6100);  // > timeout×2
        Fixture f(1, QVariant(0), Quality::Good, stale);
        QVERIFY(!f.exec.isButtonEnabled(1));
    }

    void navigatePage_emitsPageNavigationRequested()
    {
        Fixture f(1, QVariant(0));
        QSignalSpy navSpy(&f.exec, &ButtonActionExecutor::pageNavigationRequested);

        ButtonAction action;
        action.type = ButtonActionType::NavigatePage;
        action.targetPageId = 3;
        f.exec.execute(action, -1);

        QCOMPARE(navSpy.count(), 1);
        QCOMPARE(navSpy.at(0).at(0).toInt(), 3);

        // 页面跳转不依赖连接/值状态：离线时也应可用（tagId 可为 -1）。
        QSignalSpy rejectSpy(&f.exec, &ButtonActionExecutor::actionRejected);
        f.exec.setConnectionState(ConnectionState::Disconnected);
        f.exec.execute(action, -1);
        QCOMPARE(navSpy.count(), 2);
        QCOMPARE(rejectSpy.count(), 0);
    }

    void inputValue_emitsInputRequested()
    {
        Fixture f(1, QVariant(0));
        QSignalSpy inputSpy(&f.exec, &ButtonActionExecutor::inputRequested);

        ButtonAction action;
        action.type = ButtonActionType::InputValue;
        f.exec.execute(action, 1);

        QCOMPARE(inputSpy.count(), 1);
        QCOMPARE(inputSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(inputSpy.at(0).at(1).value<ButtonAction>().type, ButtonActionType::InputValue);
    }

    void execute_disabledButton_emitsActionRejected()
    {
        Fixture f(1, QVariant(0));
        f.exec.setConnectionState(ConnectionState::Disconnected);
        QSignalSpy rejectSpy(&f.exec, &ButtonActionExecutor::actionRejected);
        QSignalSpy writeSpy(&f.exec, &ButtonActionExecutor::writeRequested);

        ButtonAction action;
        action.type = ButtonActionType::FixedValue;
        action.paramA = 7;
        f.exec.execute(action, 1);

        QCOMPARE(rejectSpy.count(), 1);
        QCOMPARE(rejectSpy.at(0).at(0).toInt(), 1);
        QVERIFY(!rejectSpy.at(0).at(1).toString().isEmpty());
        QCOMPARE(writeSpy.count(), 0);
    }

    void momentary_timeoutAfter3s_autoReleases()
    {
        Fixture f(5, QVariant(0));
        ButtonAction action;
        action.type = ButtonActionType::Momentary;
        action.paramA = 1;
        action.paramB = 0;

        QSignalSpy pressSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        f.exec.execute(action, 5);
        QCOMPARE(pressSpy.count(), 1);

        // 3s 未手动释放 → 自动释放（isRelease=true, priority=1）。
        QSignalSpy releaseSpy(&f.exec, &ButtonActionExecutor::writeRequested);
        QTRY_COMPARE_WITH_TIMEOUT(releaseSpy.count(), 1, 5000);
        const WriteCommand cmd = releaseSpy.at(0).at(0).value<WriteCommand>();
        QVERIFY(cmd.isRelease);
        QCOMPARE(cmd.priority, 1);
        QCOMPARE(cmd.tagId, 5);
        QCOMPARE(cmd.value.toInt(), 0);
    }
};

int main(int argc, char* argv[])
{
    // 无显示环境下使用离屏平台（与 UI 测试一致）。
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    qRegisterMetaType<WriteCommand>();
    qRegisterMetaType<ButtonAction>();
    qRegisterMetaType<ConnectionState>();
    ButtonActionExecutorTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_ButtonActionExecutor.moc"
