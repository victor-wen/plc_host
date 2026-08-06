# 线程架构冻结：threading.md

> **冻结日期:** 2026-08-06
> **依据:** DOC-03、Qt 6.8 官方 `Threads and QObjects`（见 `docs/qt/qt-sqlite-threading.md`、`docs/qt/qt-modbus-tcp.md`）
> **状态:** 冻结。线程归属变更 = 架构变更，必须评审。

## 1. 总览

三线程架构：**UI 主线程 / 通信线程 / 数据库线程**。

```
┌─────────────────────────────┐
│ UI 主线程                    │
│  QWidget 树, QGraphicsView,  │
│  QUndoStack, Qt Charts,      │
│  ButtonActionExecutor,       │
│  TagCache 读取（快照）        │
└──────────┬──────────────────┘
           │ 信号/槽 (QueuedConnection)
           │ QMetaObject::invokeMethod
┌──────────┴──────────────────┐
│ 通信线程                     │
│  QModbusTcpClient,           │
│  IModbusClient/QtModbusClient│
│  AcquisitionEngine,          │
│  WriteQueue, PollPlanner,    │
│  ValueCodec, AlarmEngine*    │
└──────────┬──────────────────┘
           │ TagCache（QMutex 共享，非 QObject）
           │ 信号/槽（报警事件、历史样本）
┌──────────┴──────────────────┐
│ 数据库线程                   │
│  QSqlDatabase("db_thread")   │
│  DatabaseMigrator,           │
│  HistoryService, 报警落库,   │
│  DashboardRepository,        │
│  配方/日志/设置 CRUD          │
└─────────────────────────────┘
```

- 业务层只依赖接口（`IModbusClient`、仓储），不依赖 Qt 通信实现细节。
- 通信实现可替换为 libmodbus 而不改动 UI 与业务模块。

---

## 2. 线程职责与所有权（冻结）

### 2.1 UI 主线程

**拥有：**
- 所有 `QWidget`（MainWindow、TagMonitorWidget、DashboardView、AlarmWidget、RecipeWidget…）、`QGraphicsScene/View` 子类、`QUndoStack`、`QChart`/`QChartView`。
- `ButtonActionExecutor`（唯一允许产生写命令的 UI 侧对象）。
- `TagCache` 的**读取**（`snapshot()`/`value()`/`staleTagIds()`，QMutex 保护，可在 UI 线程安全调用）。
- 草稿/设置等轻量内存状态（`app_settings` 的读写经数据库线程）。

**禁止：**
- ❌ 不持有、不直接调用 `QModbusTcpClient` 或 `IModbusClient`（跨线程调用一律经 AcquisitionEngine 的槽）。
- ❌ 不持有 `QSqlDatabase`、`QSqlQuery`。所有数据库访问必须委托给数据库线程（`invokeMethod` 或经数据库线程 QObject 的槽）。
- ❌ 不直接 `new QModbusReply` 或操作通信线程对象。

### 2.2 通信线程

**拥有：**
- `QModbusTcpClient`（由 `QtModbusClient` 包装，仅在本线程构造/使用）。
- `AcquisitionEngine`（本线程 QObject；控制轮询、重连、写队列调度）。
- `WriteQueue`（引擎独占，线程内无锁）。
- `AlarmEngine`（Phase 3：评估报警规则，触发后把事件转发给数据库线程落库）。
- 无状态工具类 `ValueCodec`、`PollPlanner` 可在线程内任意使用（跨线程调用也无害，但约定只在通信线程使用）。

**约束：**
- 同一时间最多一个在途 Modbus 请求（引擎调度保证）。
- 本线程不接触任何数据库连接。
- `TagCache::updateValues` 在本线程调用（写侧）。

### 2.3 数据库线程

**拥有：**
- 独立命名连接 `QSqlDatabase("db_thread")`，仅本线程创建/打开/使用；SQLite WAL 模式 + `QSQLITE_BUSY_TIMEOUT=5000`。
- `DatabaseMigrator`（启动时在数据库线程执行迁移）。
- `HistoryService`（历史样本批量写：5s 定时或 500 条触发，事务批量 INSERT）。
- 报警事件写入（从通信线程经信号到达）。
- `DashboardRepository`、Tag/PlcConfig 仓储、RecipeService、操作日志、`app_settings` 读写。
- `BackupService`（备份/恢复，MON-07）。

**约束：**
- 连接名 `db_thread` 全局唯一；`QSqlDatabase::removeDatabase("db_thread")` 必须在所有 QSqlQuery 离开作用域、且线程退出后执行。
- 长事务必须分批（`cleanOldData` 每批 1000 行、间隔 100ms），避免长时间锁。

---

## 3. 跨线程通信规则（冻结）

1. **唯一合法机制**：信号/槽（显式或自动 `Qt::QueuedConnection`）与 `QMetaObject::invokeMethod`。
2. **禁止**直接调用其他线程中 QObject 的成员函数（含 `public slots` 的直调）；跨线程调用槽必须经队列。
3. **同步等待**：需要结果返回时用 `Qt::BlockingQueuedConnection`（UI→数据库短查询场景，禁止在 UI 线程用 `QThread::wait()` 阻塞事件循环）；数据库线程只接收短任务，避免互相死锁。
4. **元类型注册**：`TagValue`、`WriteCommand`、`ConnectionState`、`QHash<int,TagValue>` 等跨队列参数必须 `Q_DECLARE_METATYPE` 并在启动时 `qRegisterMetaType`（见 interfaces.md §0）。
5. **数据共享非 QObject**：`TagCache` 是唯一跨线程共享的数据结构（QMutex），其余一律对象私有。
6. **UI 刷新节流**：`tagValuesUpdated` 每轮询周期可能高频触发（最快 200ms）；UI 侧用 100ms 定时器合并刷新（累计缓存快照，一次 update），**UI 每 100ms 最多接收一次合并更新**。

### 3.1 跨线程通道清单（冻结）

| 方向 | 内容 | 机制 | 连接 |
|---|---|---|---|
| UI → 通信 | 连接/断开/启动/停止引擎 | `invokeMethod(engine, "start"/"stop", Qt::QueuedConnection)` | MainWindow/AppContext → AcquisitionEngine |
| UI → 通信 | 写请求（按钮/手动写入） | 信号 `writeRequested(WriteCommand)` → 槽 `enqueueWrite`（QueuedConnection） | ButtonActionExecutor → AcquisitionEngine |
| 通信 → UI | 值更新 | `tagValuesUpdated(QHash<int,TagValue>)` → UI 槽（合并节流） | AcquisitionEngine → TagMonitor/看板 |
| 通信 → UI | 连接状态 | `connectionStateChanged(ConnectionState)` | AcquisitionEngine → MainWindow 状态栏 |
| 通信 → UI | 写结果 | `writeCompleted(int,bool,QString)` | AcquisitionEngine → UI 反馈 |
| 通信 → 数据库 | 历史样本（MON-01） | `HistoryService` 在数据库线程；通信线程经信号/队列投递 | AlarmEngine/引擎 → HistoryService |
| 通信 → 数据库 | 报警事件 | `alarmEvent(...)` 信号 → 数据库线程槽（事务插入 alarm_events） | AlarmEngine → 报警落库服务 |
| UI → 数据库 | 配置/看板/配方/日志 CRUD | `invokeMethod(dbThreadWorker, ..., BlockingQueuedConnection)` 或异步槽 | UI 控制器 → 数据库线程 QObject |
| UI ↔ 数据库 | 启动迁移 | 数据库线程启动时同步执行 `DatabaseMigrator::migrate` | AppContext 启动序列 |

---

## 4. 生命周期（冻结）

### 4.1 启动（MainWindow 构造）

```
MainWindow 构造
 ├─ 创建 QThread commThread, dbThread（父 = MainWindow 或 AppContext）
 ├─ 创建 worker（无父对象）:
 │    commWorker: AcquisitionEngine(client=QtModbusClient, cache, tags)
 │    dbWorker:   DatabaseMigrator + HistoryService + 仓储门面
 ├─ worker->moveToThread(thread)
 ├─ 连接 thread->finished → worker->deleteLater
 ├─ 数据库线程: invokeMethod(migrate, BlockingQueuedConnection) —— 迁移失败则启动中止并弹错
 ├─ 连接 UI↔通信↔数据库 的全部 Queued 信号/槽
 ├─ 启动线程 (thread->start())
 └─ 按 PlcConfig.autoConnect 决定是否 invokeMethod(engine, "start")
```

### 4.2 关闭（MainWindow::closeEvent，冻结顺序）

1. `AppContext::shutdown()`：
   - **先停数据源**：`invokeMethod(engine, "stop", Qt::BlockingQueuedConnection)` —— 引擎停止轮询、abort 在途 reply、断开连接、`cancelPendingWrites()`。
   - **再停数据库**：`invokeMethod(dbWorker, "shutdown", Qt::BlockingQueuedConnection)` —— flush 待写队列（历史/报警/日志），`QSqlDatabase::removeDatabase("db_thread")`（确保无活动 QSqlQuery）。
   - `commThread->quit(); commThread->wait(3000)`；`dbThread->quit(); dbThread->wait(3000)`；超时则 `terminate()` 兜底（应仅在 bug 时发生）。
2. 线程已退出 → 删除 worker（`finished`→`deleteLater` 已挂接）。
3. `event->accept()`，MainWindow 及 UI 树正常析构。
4. **顺序不变量**：先停通信线程，再停数据库线程，最后销毁 UI。绝不在 UI 析构后仍让线程访问 UI 对象（worker 连接 UI 的槽全部断开或保证 UI 在退出前移除连接）。

### 4.3 重新连接 / 运行中切换配置

- 运行中改 PlcConfig：`stop` → 重建引擎内 tags/配置 → `start`（全部经队列）。
- 手动断开后引擎不自动重连（状态机见 interfaces.md §8）。

---

## 5. 线程安全细则（冻结）

| 对象 | 归属线程 | 并发访问规则 |
|---|---|---|
| QWidget / QGraphicsScene / QUndoStack / QChart | UI | 仅 UI 线程；不得跨线程碰 |
| IModbusClient / QModbusTcpClient | 通信 | 仅通信线程；UI 不得持有指针 |
| AcquisitionEngine | 通信 | 仅通信线程执行；其他线程仅经队列槽调用 |
| WriteQueue | 通信 | 引擎独占，无锁 |
| AlarmEngine | 通信 | 仅通信线程 |
| TagCache | 共享 | QMutex；通信写 / UI 读；永不返回内部引用 |
| HistoryService / 仓储 / Migrator | 数据库 | 仅数据库线程；其他线程经 invokeMethod |
| QSqlDatabase("db_thread") | 数据库 | 仅数据库线程；复制 QSqlDatabase 不创建新连接 |
| ValueCodec / PollPlanner | 任意 | 静态无状态，线程安全 |
| ButtonActionExecutor | UI | 仅 UI 线程 |

---

## 6. 反模式清单（禁止，review 时逐条核对）

1. ❌ 在 UI 线程 `client->sendReadRequest(...)` 或持有 `QModbusReply` 指针。
2. ❌ 在线程间传递 `QSqlDatabase`/`QSqlQuery` 值拷贝。
3. ❌ 跨线程直调 `public slots`（未走队列）。
4. ❌ 在 UI 线程用 `QThread::msleep` 或 `wait()` 长阻塞（事件循环饥饿）。
5. ❌ 数据库线程执行长查询阻塞 UI 同步请求（超时 3s 防护）。
6. ❌ `terminate()` 作为常规关闭手段（仅超时兜底）。
7. ❌ 编辑模式下经任何路径产生写命令（DASH-09 验证：拦截 sendWriteRequest 计数 = 0）。
8. ❌ 通信线程直接操作 UI 对象（如调用 widget->setText）；一律经信号。
