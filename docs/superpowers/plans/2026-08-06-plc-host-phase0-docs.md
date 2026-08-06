# Phase 0: Qt 文档与架构冻结

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 建立 Qt 6.8 本地参考文档并冻结所有接口定义。

**Tech Stack:** Context7, Qt 6.8 官方文档站

## Global Constraints

- Qt 6.8 LTS only，不混用 Qt 5 或其他 Qt 6 版本。
- 所有文档标注官方 URL 和拉取日期。
- 单台 PLC，最多 1000 Tag，默认 500ms 采集。
- QObject 跨线程仅用信号/槽或 QMetaObject::invokeMethod。
- SQLite 每线程独立连接，WAL 模式。
- 编辑模式严禁 PLC 写请求。

---

### Task DOC-01: 拉取 Qt 核心模块文档

**Agent:** `@frontend` (opencode-go/gpt-5.6-luna)
**Priority:** 最高，必须最先执行
**Parallel:** 可与后续任何任务并行（纯文档，不写代码）

**Files to create:**
- `docs/qt/README.md`
- `docs/qt/qt-modbus-tcp.md`
- `docs/qt/qt-widgets-graphics-view.md`
- `docs/qt/qt-undo-framework.md`
- `docs/qt/qt-sqlite-threading.md`
- `docs/qt/qt-test-framework.md`
- `docs/qt/qt-charts.md`
- `docs/qt/qt-windows-deployment.md`

**Constraints:**
- 仅使用 Context7 和 doc.qt.io 官方文档。
- 每份文档标注来源 URL、Qt 版本(6.8)、拉取日期(2026-08-06)。
- 不完整镜像 Qt 网站，只保存项目需要的 API 和行为说明。
- 中文或中英对照均可。

**Required sections per document:**

`qt-modbus-tcp.md`:
- QModbusTcpClient 构造、连接参数、open/close、sendReadRequest/sendWriteRequest、QModbusReply 生命周期和信号、setTimeout/setNumberOfRetries、stateChanged/errorOccurred
- QModbusDataUnit RegisterType 枚举和地址操作
- 线程安全规则: QObject 线程亲和性，跨线程必须信号/槽
- 并发的在途请求限制和兼容性约束
- 错误码列表和 keep-alive 行为

`qt-widgets-graphics-view.md`:
- QGraphicsScene: 坐标系、item 管理、选中、事件传播、索引算法
- QGraphicsView: 视图变换、渲染提示、dragMode、视口和滚动
- QGraphicsObject: 信号/槽支持、变换方法、boundingRect/shape/paint 职责、flags、itemChange、zValue 层级
- 自定义项 paint 模式、坐标系统、DeviceCoordinateCache vs NoCache

`qt-undo-framework.md`:
- QUndoStack: push/undo/redo/isClean/canUndo/canRedo、cleanChanged 信号、栈大小
- QUndoCommand: undo/redo/id/mergeWith、setText、子命令和宏命令
- 典型 undo command 模式: 构造函数保存旧状态，redo 应用新状态，undo 恢复

`qt-sqlite-threading.md`:
- QSqlDatabase 每线程连接要求、不能跨线程共享
- WAL 模式启用语句和并发行为
- 批量写入事务策略、busy_timeout
- 命名连接和 migrate 模式

`qt-test-framework.md`:
- QVERIFY/QCOMPARE/QTRY_VERIFY_QTRY_COMPARE/QSignalSpy
- 测试类结构: init/cleanup/initTestCase/cleanupTestCase
- 数据驱动测试、CMake/qt_add_test、CTest 集成

`qt-charts.md`:
- QChart/QChartView/QLineSeries/QValueAxis/QDateTimeAxis
- 实时更新模式: 追加点、限制点数、animation 关闭
- CSV 导出集成方式

`qt-windows-deployment.md`:
- windeployqt 命令格式和模块参数
- CMake install + qt_generate_deploy_app_script
- MSVC 运行时和常见缺失 DLL

**Commit:**
```
git add docs/qt/
git commit -m "docs: add Qt 6.8 local reference documentation"
```

---

### Task DOC-02: 文档准确性审查

**Agent:** `@docs-scout` (deepseek-v4-flash)
**Depends on:** DOC-01

审查 docs/qt/*.md:
- 引用是否来自 doc.qt.io 官方站
- 是否混入 Qt 5 或其他 Qt 6 版本 API
- 线程约束是否与官方文档一致

输出审查结论: 通过 / 需修正。如需修正，@frontend 更新后重新审查。

---

### Task DOC-03: 架构冻结

**Agent:** `@architect` (deepseek-v4-flash)
**Depends on:** DOC-02 通过

**Files to create:**
- `docs/architecture/interfaces.md`
- `docs/architecture/threading.md`
- `docs/architecture/database-schema.md`

**interfaces.md must define:**

```cpp
// IModbusClient - 抽象基类，QObject in 通信线程
class IModbusClient : public QObject {
    Q_OBJECT
public:
    virtual void connectToDevice(const QString& host, int port, int unitId) = 0;
    virtual void disconnectFromDevice() = 0;
    virtual bool isConnected() const = 0;
    virtual QModbusReply* sendReadRequest(const QModbusDataUnit& unit, int serverAddress) = 0;
    virtual QModbusReply* sendWriteRequest(const QModbusDataUnit& unit, int serverAddress) = 0;
    virtual void setTimeout(int ms) = 0;
    virtual void setNumberOfRetries(int n) = 0;
signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
};

// ValueCodec - 无状态工具类
struct DecodeResult { QVariant value; bool valid = false; QString error; };
ValueCodec::decode(QModbusDataUnit, Tag) -> DecodeResult
ValueCodec::encode(Tag, QVariant) -> QModbusDataUnit
ValueCodec::modbusAddress(Tag) -> int  // 零基 PDU 地址

// PollPlanner
struct PollGroup { RegisterType; int startAddress, count, intervalMs; QList<int> tagIds; };
PollPlanner::buildGroups(QList<Tag>) -> QList<PollGroup>

// AcquisitionEngine - 通信线程 QObject
void start(); void stop();
void enqueueWrite(WriteCommand); void cancelPendingWrites();
signals: tagValuesUpdated(QHash<int,TagValue>), connectionStateChanged(ConnectionState), writeCompleted(int,bool,QString)

// TagCache - 线程安全的值缓存
void updateValues(QHash<int,TagValue>)
QHash<int,TagValue> snapshot() const
TagValue value(int tagId) const
QList<int> staleTagIds(int thresholdMs) const

// WriteQueue
struct WriteCommand { QUuid id; int tagId; QVariant value; QDateTime createdAt; int expiryMs=5000; bool isRelease=false; int priority=0; };
void enqueue(WriteCommand); void clear(); optional<WriteCommand> dequeue(); void removeExpired(int nowMs); bool isEmpty();

// DashboardPage { id, name, width, height, background, sortOrder }
// DashboardItem { id, pageId, itemType, x, y, width, height, zOrder, commonStyle(QJsonObject), config(QJsonObject), schemaVersion }
// DashboardRepository { loadPages, savePage, deletePage, loadItems, saveItems }

// ButtonAction - 五种类型: Momentary, Toggle, FixedValue, InputValue, NavigatePage
// ButtonActionExecutor (UI 线程) - execute/releaseMomentary/releaseAllMomentary/isButtonEnabled
// signals: writeRequested(WriteCommand), pageNavigationRequested(int)

// ConnectionState: Disconnected, Connecting, Online, Degraded, Reconnecting
// Quality: Good, Stale, Bad, Disconnected
```

**threading.md must define:**
- UI 主线程: 所有 QWidget, ButtonActionExecutor, TagCache 读取; 不持有 QModbusTcpClient 和 QSqlDatabase
- 通信线程: QModbusTcpClient, AcquisitionEngine, WriteQueue; 无状态工具类随意
- 数据库线程: 独立 QSqlDatabase("db_thread"); 历史、报警、日志写入
- 跨线程: signal/slot (QueuedConnection), QMetaObject::invokeMethod
- 生命周期: MainWindow 构造时创建线程，closeEvent 先 stop 线程再销毁 UI

**database-schema.md must define:**
- V1 迁移: schema_migrations, plc_config(单行CHECK id=1), tags, dashboard_pages, dashboard_items, alarm_rules, alarm_events, history_samples(含索引), recipes, recipe_items, operation_logs, app_settings
- 所有字段类型、默认值、外键和级联规则
- V2+ 迁移方式说明

**Review gate:** @reviewer 审查 interfaces.md 的接口无歧义性、线程约束、信号参数和错误路径。

**Commit:**
```
git add docs/architecture/
git commit -m "docs: architecture freeze - interfaces, threading, schema"
```
