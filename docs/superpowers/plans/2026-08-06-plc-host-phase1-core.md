# Phase 1: 核心通信层

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 建立 CMake 工程、领域模型、Modbus TCP 通信、Tag 采集引擎和安全写入队列，实现 PLC 连接配置、Tag 编辑和实时变量表界面。

**Architecture:** 三线程分离(UI/通信/数据库)，IModbusClient 抽象隔离 Qt 通信库，ValueCodec 处理字节序和类型转换，PollPlanner 合并连续地址，AcquisitionEngine 管理轮询调度。

**Tech Stack:** C++20, Qt 6.8 Widgets, Qt SerialBus, Qt SQL, SQLite WAL, CMake 3.24+, MSVC 2022 x64, Ninja, Qt Test, CTest

## Global Constraints

- C++20, Qt 6.8 LTS only. MSVC 2022 x64 + Ninja build.
- 1 台 PLC, 最多 1000 Tag, 默认 500ms, 少量连续地址 200ms.
- 零基 PDU 地址内部存储，界面可选零基/一基输入。
- 同一时间最多一个在途 Modbus 请求。
- 编辑模式严禁 PLC 写请求。
- 每项任务编写失败测试 → 确认失败 → 实现 → 通过 → 全量 ctest → @reviewer 审查 → 修复 → 复审。
- 每次提交独立且可通过 ctest。

## 目录结构

```
plc_host/
├── CMakeLists.txt              # 根: project, find_package, add_subdirectory
├── cmake/Warnings.cmake        # /W4 /WX
├── src/
│   ├── app/
│   │   ├── main.cpp            # QApplication, MainWindow
│   │   ├── MainWindow.h/cpp    # 主窗口
│   │   └── AppContext.h/cpp    # 全局上下文
│   ├── domain/
│   │   ├── PlcConfig.h         # PLC 连接参数
│   │   ├── Tag.h               # 变量定义
│   │   └── TagValue.h          # 运行时值 + 质量
│   ├── modbus/
│   │   ├── IModbusClient.h
│   │   ├── QtModbusClient.h/cpp
│   │   ├── ValueCodec.h/cpp
│   │   └── PollPlanner.h/cpp
│   ├── runtime/
│   │   ├── AcquisitionEngine.h/cpp
│   │   ├── TagCache.h/cpp
│   │   └── WriteQueue.h/cpp
│   └── storage/
│       └── DatabaseMigrator.h/cpp
└── tests/
    ├── CMakeLists.txt
    ├── unit/
    │   ├── tst_ValueCodec.cpp
    │   ├── tst_PollPlanner.cpp
    │   ├── tst_WriteQueue.cpp
    │   └── tst_TagCache.cpp
    └── integration/
        └── tst_QtModbusClient.cpp
```

---

### Task CORE-01: 项目骨架、CMake 和烟雾测试

**Agent:** `@coder` (deepseek-v4-flash)
**Depends on:** DOC-03 接口冻结
**Parallel:** 无，此任务建立目录，必须最先执行

**Files to create:**
- `CMakeLists.txt`
- `cmake/Warnings.cmake`
- `src/app/main.cpp`
- `src/app/AppContext.h`
- `tests/CMakeLists.txt`
- `tests/unit/tst_smoke.cpp`

**CMakeLists.txt 必须包含:**
```cmake
cmake_minimum_required(VERSION 3.24)
project(plc_host VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets SerialBus Sql Test Charts)
enable_testing()
include(cmake/Warnings.cmake)
qt_standard_project_setup()
add_subdirectory(src)
add_subdirectory(tests)
```

**Warnings.cmake:**
```cmake
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()
```

**main.cpp:**
```cpp
#include <QApplication>
#include "app/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("PLC Host");
    app.setApplicationVersion("0.1.0");
    MainWindow window;
    window.show();
    return app.exec();
}
```

**tst_smoke.cpp:**
```cpp
#include <QTest>
#include <QApplication>

class SmokeTest : public QObject {
    Q_OBJECT
private slots:
    void alwaysPasses() {
        QVERIFY(true);
    }
};

QTEST_MAIN(SmokeTest)
#include "tst_smoke.moc"
```

**tests/CMakeLists.txt:**
```cmake
function(add_qtest name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_qtest(unit/tst_smoke)
```

**Verification:**
```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$env:QT_ROOT -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

**Commit:** `feat: project skeleton with CMake, smoke test, and main entry`

---

### Task CORE-02: 领域模型与数据库迁移

**Agent:** `@coder` (deepseek-v4-flash)
**Depends on:** CORE-01

**Files to create:**
- `src/domain/PlcConfig.h`
- `src/domain/Tag.h`
- `src/domain/TagValue.h`
- `src/storage/DatabaseMigrator.h`
- `src/storage/DatabaseMigrator.cpp`
- `tests/unit/tst_DomainModels.cpp`
- `tests/unit/tst_DatabaseMigrations.cpp`

**PlcConfig.h:**
```cpp
#pragma once
#include <QString>
struct PlcConfig {
    int id = 1;
    QString name;
    QString host = "192.168.1.100";
    int port = 502;
    int unitId = 1;
    int timeoutMs = 1000;
    int retries = 2;
    int pollIntervalMs = 500;
    bool autoConnect = true;
};
```

**Tag.h:**
```cpp
#pragma once
#include <QString>
#include <cstdint>

enum class RegisterType : int { Coil = 0, DiscreteInput = 1, InputRegister = 2, HoldingRegister = 3 };
enum class DataType : int { Bool = 0, Int16 = 1, UInt16 = 2, Int32 = 3, UInt32 = 4, Float32 = 5, BitField = 6 };
enum class ByteOrder : int { ABCD = 0, DCBA = 1, BADC = 2, CDAB = 3 };
enum class WordOrder : int { HighLow = 0, LowHigh = 1 };
enum class HistoryMode : int { Periodic = 0, OnChange = 1 };

struct Tag {
    int id = -1;
    QString name;
    RegisterType registerType = RegisterType::HoldingRegister;
    int address = 0;            // 零基 PDU 地址
    DataType dataType = DataType::UInt16;
    ByteOrder byteOrder = ByteOrder::ABCD;
    WordOrder wordOrder = WordOrder::HighLow;
    int bitPosition = 0;
    int bitLength = 1;
    double scale = 1.0;
    double offset = 0.0;
    QString unit;
    bool readOnly = false;
    int pollGroup = 0;
    int pollIntervalMs = 500;
    bool historyEnabled = false;
    HistoryMode historyMode = HistoryMode::Periodic;

    int registerCount() const;      // 此 Tag 占用的寄存器数
    int modbusAddress() const;      // 返回 address (零基)
};
```

**TagValue.h:**
```cpp
#pragma once
#include <QVariant>
#include <QDateTime>
#include <QString>

enum class Quality { Good, Stale, Bad, Disconnected };

struct TagValue {
    int tagId = -1;
    QVariant value;
    QVariant rawValue;
    Quality quality = Quality::Disconnected;
    QDateTime timestamp;
    QString error;
};
```

**DatabaseMigrator.h/cpp:**
- `bool migrate(QSqlDatabase& db)` 执行所有迁移。
- V1: 创建 spec 定义的全部表。
- 使用事务保护每次迁移。
- `currentVersion(QSqlDatabase& db) -> int` 返回当前 schema 版本。

**tst_DomainModels.cpp 测试:**
- Tag::registerCount() 对 Bool/Int16/UInt16 返回 1，对 Int32/UInt32/Float32 返回 2
- Tag::modbusAddress() 返回 address
- TagValue 默认 quality 为 Disconnected

**tst_DatabaseMigrations.cpp 测试:**
- 使用临时 SQLite 文件数据库
- V1 迁移后所有表存在且结构与 spec 一致
- currentVersion 返回正确版本
- 重复迁移幂等

**Verification:**
```powershell
ctest --test-dir build -R "DomainModels|DatabaseMigrations" --output-on-failure
```

**Commit:** `feat: domain models (PlcConfig, Tag, TagValue) and V1 database migration`

---

### Task CORE-03: 地址转换与值编解码

**Agent:** `@coder` (deepseek-v4-flash)
**Depends on:** CORE-02

**Files to create:**
- `src/modbus/ValueCodec.h`
- `src/modbus/ValueCodec.cpp`
- `tests/unit/tst_ValueCodec.cpp`

**ValueCodec 测试覆盖 (tst_ValueCodec.cpp):**

测试地址转换:
- 零基地址 0 → modbusAddress 返回 0
- 一基地址转换: 保持内部零基不变，只是 UI 输入转换

测试 UInt16 编解码 (ABCD, 值 4660 = 0x1234):
```cpp
void testDecodeUInt16_ABCD() {
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters);
    unit.setValues({0x1234});
    Tag tag; tag.dataType = DataType::UInt16; tag.byteOrder = ByteOrder::ABCD;
    auto result = ValueCodec::decode(unit, tag);
    QVERIFY(result.valid);
    QCOMPARE(result.value.toInt(), 4660);
}
```

测试 Float32 四种字节序:
- ABCD (big-endian): 寄存器 [0x3F80, 0x0000] → 1.0f
- DCBA (little-endian swapped): [0x0000, 0x3F80] → 1.0f
- BADC: [0x803F, 0x0000] → 1.0f
- CDAB: [0x0000, 0x803F] → 1.0f

测试 Int32/UInt32:
- Int32 负值 (-1 = 0xFFFF, 0xFFFF)
- UInt32 最大值 (0xFFFFFFFF)

测试 Bool 线圈:
- coil 值为 1 → true, 0 → false

测试 BitField:
- bitPosition=3, bitLength=1, 寄存器值 0x0008 → 提取 bit3 为 1

测试编码:
- encode 后 QModbusDataUnit 值匹配输入

测试边界:
- 单位不足时报错
- Tag 地址超出返回范围时 decode 报错
- 无效数据类型报错

**Commit:** `feat: ValueCodec with address conversion and all byte-order/type codecs`

---

### Task CORE-04: 轮询规划器

**Agent:** `@coder` (deepseek-v4-flash)
**Depends on:** CORE-02

**Files to create:**
- `src/modbus/PollPlanner.h`
- `src/modbus/PollPlanner.cpp`
- `tests/unit/tst_PollPlanner.cpp`

**PollPlanner::buildGroups 规则:**
- 按 RegisterType 分组
- 每组内按地址排序
- 相邻且间隔 <= 5 个寄存器的地址合并为一个读取块
- 单块不超过 125 个寄存器
- 按 pollIntervalMs 分组

**tst_PollPlanner.cpp 测试:**
- 单一 Tag 生成一个 PollGroup
- 两个连续地址 Tag 合并为一块
- 两个间隔 >5 的 Tag 拆为两块
- 不同 RegisterType 的 Tag 分到不同组
- 不同 pollIntervalMs 的 Tag 分到不同组
- 超过 125 寄存器时分块
- 空 Tag 列表返回空组

**Commit:** `feat: PollPlanner with contiguous address merging and interval grouping`

---

### Task CORE-05: IModbusClient 与 QtModbusClient

**Agent:** `@coder` (deepseek-v4-flash)
**Depends on:** CORE-02

**Files to create:**
- `src/modbus/IModbusClient.h`
- `src/modbus/QtModbusClient.h`
- `src/modbus/QtModbusClient.cpp`
- `tests/integration/tst_QtModbusClient.cpp`

**IModbusClient.h:** 按 DOC-03 定义的纯虚接口。

**QtModbusClient:**
- 包装 QModbusTcpClient
- connectToDevice 调用 setConnectionParameter + open
- 状态变化和错误通过信号转发
- sendReadRequest/sendWriteRequest 委托给 QModbusTcpClient

**tst_QtModbusClient.cpp 集成测试:**
- 使用 QSignalSpy 验证 connected/disconnected 信号
- 使用假服务器或 mock 测试超时和错误
- 注意: 此测试在没有真实 PLC 时使用 QModbusTcpServer 在 localhost 上模拟

**Commit:** `feat: IModbusClient interface and QtModbusClient implementation`

---

### Task CORE-06: TagCache 与质量状态

**Agent:** `@coder` (deepseek-v4-flash)
**Depends on:** CORE-02

**Files to create:**
- `src/runtime/TagCache.h`
- `src/runtime/TagCache.cpp`
- `tests/unit/tst_TagCache.cpp`

**TagCache 线程安全要求:**
- updateValues 和 snapshot 互斥 (QMutex)
- snapshot 复制当前所有值
- staleTagIds 返回 timestamp 早于 (now - thresholdMs) 的 tag

**tst_TagCache.cpp 测试:**
- 空缓存 snapshot 返回空
- updateValues 后 snapshot 包含正确值
- 部分更新不丢失未更新的值
- staleTagIds 正确识别过期值
- Quality 状态正确传递

**Commit:** `feat: thread-safe TagCache with quality state management`

---

### Task CORE-07: WriteQueue 与写入安全

**Agent:** `@coder` (deepseek-v4-flash)
**Depends on:** CORE-02

**Files to create:**
- `src/runtime/WriteQueue.h`
- `src/runtime/WriteQueue.cpp`
- `tests/unit/tst_WriteQueue.cpp`

**WriteQueue 行为:**
- 高优先级 (priority=1, 点动释放) 总是先于普通优先级出队
- 同时刻入队的命令 FIFO
- removeExpired 丢弃 createdAt + expiryMs < nowMs 的命令
- clear 丢弃所有待处理命令

**tst_WriteQueue.cpp 测试:**
- 空队列 isEmpty 返回 true
- enqueue/dequeue FIFO
- 高优先级插队
- removeExpired 只丢弃过期命令
- clear 排空队列
- 点动释放 (isRelease=true) 自动设 priority=1

**Commit:** `feat: WriteQueue with priority, expiry, and bulk clear`

---

### Task CORE-08: AcquisitionEngine

**Agent:** `@coder` (deepseek-v4-flash)
**Depends on:** CORE-03, CORE-04, CORE-05, CORE-06, CORE-07

**Files to create:**
- `src/runtime/AcquisitionEngine.h`
- `src/runtime/AcquisitionEngine.cpp`
- `tests/integration/tst_AcquisitionEngine.cpp`

**AcquisitionEngine 核心逻辑:**
- 构造时接收 IModbusClient*, TagCache*, QList<Tag>
- start(): 建 PollGroup, 开始轮询, 追踪 ConnectionState
- 轮询: 按 PollGroup 的 intervalMs 定时发送读请求
- 收到回复: ValueCodec::decode, TagCache::updateValues, emit tagValuesUpdated
- 错误: 单次失败标记 Stale, 连续失败标记 Bad→Disconnected
- 重连: 退避 1/2/4/8/16/30s
- 手动断开: 不自动重连
- enqueueWrite: 加入 WriteQueue, 在两次轮询间隙处理
- cancelPendingWrites: 断开时调用

**tst_AcquisitionEngine.cpp 集成测试:**
- 使用 IModbusClient 的 Fake 实现
- 验证轮询周期发送读请求
- 验证 decode 后的值到达 TagCache
- 验证 connectionStateChanged 信号序列
- 验证写入优先级
- 验证断线后写队列清空

**Commit:** `feat: AcquisitionEngine with poll scheduling, reconnect backoff, and write queue`

---

### Task CORE-09: PLC 配置页、Tag 编辑页、实时变量表

**Agent:** `@coder` (deepseek-v4-flash)
**Depends on:** CORE-01 (ui shell)

**Files to create:**
- `src/app/MainWindow.h/cpp`
- `src/ui/PlcConfigWidget.h/cpp`
- `src/ui/TagEditorWidget.h/cpp`
- `src/ui/TagMonitorWidget.h/cpp`
- `tests/ui/tst_UiSmoke.cpp`

**MainWindow:**
- QTabWidget 或 QStackedWidget 组织页面
- 顶部状态栏显示连接状态

**PlcConfigWidget:**
- IP、端口、Unit ID、超时、重试、采集周期、自动连接
- 连接/断开按钮
- 状态指示灯 (Good=绿, Degraded=黄, Bad/Disconnected=红)

**TagEditorWidget:**
- QTableView + 模型
- 列: 名称、寄存器类型(下拉)、地址、数据类型(下拉)、字节序、字序、倍率、偏移、单位、读写、采集组、周期
- 新增/删除/编辑 Tag
- 手动写入: 双击值列 → 校验类型和范围 → 确认 → 发送写入
- 导入/导出 CSV

**TagMonitorWidget:**
- QTableView 显示实时 TagValue
- 质量状态列颜色: Good=绿, Stale=黄, Bad=红, Disconnected=灰
- 搜索和筛选

**Commit:** `feat: PLC config, tag editor, and real-time tag monitor UI`

---

## Phase 1 审查节点

| 节点 | 触发条件 | 审查重点 |
|---|---|---|
| RG-1 | CORE-04 完成 | 地址编解码边界、合并逻辑、Modbus 单次上限 |
| RG-2 | CORE-07 完成 | 旧命令重放、点动安全、并发访问、过期机制 |
| RG-3 | CORE-09 完成 | 全量 ctest + @reviewer 审查全部核心模块 |

审查通过后 Phase 1 完成，可进入 Phase 2 看板或 Phase 3 监控模块。
