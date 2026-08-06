# Qt Test 测试框架

> Qt 版本：6.8（官方页面当前显示的补丁版本为 6.8.8）
> 拉取日期：2026-08-06
> 来源：<https://doc.qt.io/qt-6.8/qttest-index.html>、<https://doc.qt.io/qt-6.8/qtest.html>、<https://doc.qt.io/qt-6.8/qsignalspy.html>、<https://doc.qt.io/qt-6.8/qtest-overview.html>、<https://doc.qt.io/qt-6.8/qttestlib-tutorial1-example.html>、<https://doc.qt.io/qt-6.8/qttestlib-tutorial2-example.html>、<https://doc.qt.io/qt-6.8/cmake-deployment.html>、<https://doc.qt.io/qt-6.8/threads-qobject.html>
> Context7 libraryId：`/websites/doc_qt_io_qt-6`

## 模块和最小测试类

```cpp
find_package(Qt6 REQUIRED COMPONENTS Test)
target_link_libraries(mytarget PRIVATE Qt6::Test)
```

Qt Test 通过 `QObject` 的元对象发现测试槽。测试类应使用 `Q_OBJECT`，测试函数放在 `private slots:`：

```cpp
#include <QTest>

class TestValueCodec final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void decodesUInt16();
    void decodesUInt16_data();
};

QTEST_MAIN(TestValueCodec)
#include "tst_valuecodec.moc"
```

`QTEST_MAIN(TestClass)` 生成 `main()` 并运行测试。若声明和实现都在同一个 `.cpp` 中，启用 CMake AUTOMOC 后仍按官方教程在文件末尾包含生成的 `.moc` 文件。

生命周期槽的顺序和职责：

| 槽 | 调用时机 | 适合做什么 |
|---|---|---|
| `initTestCase()` | 第一个测试前一次 | 初始化整个测试夹具；失败时不执行测试函数 |
| `initTestCase_data()` | 第一个测试前一次 | 建立全局数据表 |
| `init()` | 每个测试函数前 | 初始化单个测试资源 |
| `cleanup()` | 每个测试函数后 | 清理单个测试资源，即使测试提前失败也会调用 |
| `cleanupTestCase()` | 最后一个测试后一次 | 清理整个测试夹具 |

RAII 也可用于保证局部资源清理。每个测试应将系统恢复到可重复运行的状态。

## 断言和异步等待

### QVERIFY 和 QCOMPARE

```cpp
QVERIFY(condition);
QVERIFY2(condition, "diagnostic message");
QCOMPARE(actual, expected);
```

- `QVERIFY()` 求值条件；为真则继续，为假记录失败并结束当前测试函数。
- `QCOMPARE()` 比较两个值；失败时会输出实际值和期望值，通常比手写 `QVERIFY(actual == expected)` 更容易定位问题。
- 两者都不应替代生产代码的错误处理；断言失败只影响测试执行流。

### QTRY_VERIFY 和 QTRY_COMPARE

异步信号、定时器或 queued slot 还未执行时，不能用固定的 `QVERIFY()` 立即断言。Qt Test 提供会运行事件循环并重复检查的宏：

```cpp
QTRY_VERIFY(cache->quality(tagId) == Quality::Good);
QTRY_VERIFY_WITH_TIMEOUT(worker->isReady(), 2000);
QTRY_COMPARE(spy.count(), 1);
QTRY_COMPARE_WITH_TIMEOUT(reply->error(), QModbusDevice::NoError, 3000);
```

`QTRY_VERIFY(condition)` 在条件为真前反复求值，或到达默认超时后失败；`QTRY_VERIFY_WITH_TIMEOUT` 允许显式指定毫秒超时。`QTRY_COMPARE`/`QTRY_COMPARE_WITH_TIMEOUT` 对比较表达式执行相同的重试。测试必须给异步条件设置有意义的上限，避免无限等待；条件内部不要产生不可重复的副作用。

## QSignalSpy

`QSignalSpy` 监听任意 `QObject` 信号，并把每次发射记录为一个 `QVariant` 列表。优先使用指向成员函数的类型安全构造函数：

```cpp
QSignalSpy spy(worker, &Worker::completed);
QVERIFY(spy.isValid());

worker->start();
QTRY_COMPARE(spy.count(), 1);
const QList<QVariant> arguments = spy.takeFirst();
QCOMPARE(arguments.at(0).toInt(), 42);
```

常用签名：

```cpp
QSignalSpy(const QObject *object, PointerToMemberFunction signal);

bool isValid() const;
bool wait(int timeout);
bool wait(std::chrono::milliseconds timeout = std::chrono::seconds{5});
```

`QVERIFY(spy.isValid())` 应紧跟构造之后。无效通常表示对象为空或信号不存在。自定义参数类型必须先 `qRegisterMetaType<T>()`，否则 queued 信号和 spy 可能无法存储参数。`wait()` 至少等到一次信号或超时；Qt 6.6 起有 chrono 重载，默认 5 秒。

## 数据驱动测试

关联函数名加 `_data`，在其中定义列和行：

```cpp
void TestValueCodec::decodesUInt16_data()
{
    QTest::addColumn<quint16>("raw");
    QTest::addColumn<int>("expected");

    QTest::newRow("zero") << quint16(0) << 0;
    QTest::newRow("max") << quint16(65535) << 65535;
}

void TestValueCodec::decodesUInt16()
{
    QFETCH(quint16, raw);
    QFETCH(int, expected);
    QCOMPARE(decode(raw), expected);
}
```

测试函数会对每一行运行一次。`QTest::addColumn<T>()` 声明列，`newRow()`/`addRow()` 添加数据，`QFETCH()` 取出值；列名和行名应唯一。全局数据可在 `initTestCase_data()` 定义，并用 `QFETCH_GLOBAL()` 读取。

## CMake 和 CTest

Qt 6.8 官方教程使用 `qt_add_executable()` 构建测试程序，再用 CMake 的 `add_test()` 注册到 CTest：

```cmake
cmake_minimum_required(VERSION 3.24)
project(plc_host_tests LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Test)
qt_standard_project_setup()

qt_add_executable(test_valuecodec
    tst_valuecodec.cpp
)
target_link_libraries(test_valuecodec PRIVATE
    Qt6::Core
    Qt6::Test
)

include(CTest)
enable_testing()
add_test(NAME test_valuecodec COMMAND test_valuecodec)
```

配置并运行：

```text
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### 关于 `qt_add_test`

Qt 6.8 官方 CMake 命令参考没有名为 `qt_add_test` 的内置命令；官方 Test 教程的可移植写法是 `qt_add_executable` + `add_test`。因此本项目不能把 `qt_add_test(...)` 当成 Qt 6.8 API 直接调用。

如果项目未来定义了自有 CMake 封装，可以使用这个名字，但必须明确它是项目宏，并在内部完成同样的目标创建、`Qt6::Test` 链接和 `add_test()` 注册：

```cmake
# 仅表示项目自定义宏；Qt 6.8 本身不提供它
qt_add_test(test_valuecodec tst_valuecodec.cpp)
```

没有宏定义时，这段配置应被视为错误；不要为了迎合命名而引入一个未定义的 Qt 命令。

CTest 可以按测试名称正则筛选，并可用 CTest `LABELS` 属性分组；Qt Test 可输出普通文本、XML 或 JUnit XML，便于 CI 收集结果。

## 看板项目测试建议

- 编码/地址/迁移等纯逻辑用无 GUI 测试入口；Graphics View 和 QWidget 测试使用 `QTEST_MAIN` 或相应 GUI 应用环境。
- Modbus reply、数据库 worker 和定时器使用 `QSignalSpy` + `QTRY_*`，不要用 `QTest::qWait()` 作为唯一同步手段。
- 断言异步对象的最终状态和错误文本；同时验证重复信号、超时和对象销毁路径。
- UI 测试可以使用 `QTest::mouseClick()`、`keyClick()` 等内部 Qt 事件模拟；需要显示的控件应先 `show()`。

## 线程安全约束

Qt Test 的错误报告本身是线程安全的，但被测 QObject 不是自动线程安全的。测试必须在被测对象所属线程中读写其状态；`QWidget`、`QGraphicsView` 和 Qt Charts 只在 UI 主线程测试。

跨线程只使用信号/槽（必要时 queued connection），并让接收线程运行事件循环；不要在测试线程直接调用工作线程对象的槽或销毁它。`QSignalSpy` 应在正确的对象线程中创建并验证，结束测试时先停止 worker，再等待线程退出。
