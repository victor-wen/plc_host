# 接口冻结：interfaces.md

> **冻结日期:** 2026-08-06
> **依据:** `docs/superpowers/plans/2026-08-06-plc-host-phase0-docs.md` (DOC-03)、Phase 1/2/3 实施计划、Qt 6.8 官方文档 (见 `docs/qt/`)
> **状态:** 冻结。后续修改必须走接口变更评审，并同步更新本文件与相关阶段计划。

## 0. 全局约定

- **技术基线:** C++20, Qt 6.8 LTS。所有 `QObject` 遵循线程亲和性（详见 `threading.md`）。
- **线程归属:** 每个接口标注所属线程。跨线程只允许 信号/槽（`Qt::QueuedConnection`）或 `QMetaObject::invokeMethod`。
- **元类型注册:** 为保证 QueuedConnection 可用，以下类型必须注册 `Q_DECLARE_METATYPE` + `qRegisterMetaType`（应用启动时注册一次）：
  `TagValue`、`WriteCommand`、`DecodeResult`、`ConnectionState`、`QHash<int,TagValue>`。
- **零基地址:** 内部存储一律使用零基 PDU 地址；界面层可选零基/一基输入（仅 UI 层转换）。
- **错误语义:** 所有错误通过 `QString error` 携带人类可读信息；接口不抛异常（除构造期资源失败）。
- **头文件位置:** 按 `AGENTS.md` 目录结构放置；接口头文件只依赖领域模型和 Qt 基础类型，不依赖实现细节。

---

## 1. 领域基础类型（冻结）

头文件：`src/domain/PlcConfig.h`、`src/domain/Tag.h`、`src/domain/TagValue.h`（Phase 1 CORE-02 定义，此处为引用并冻结）。

```cpp
// PlcConfig.h —— 单台 PLC 连接配置（数据库 plc_config 表，id 固定 = 1）
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

// Tag.h —— 变量定义
enum class RegisterType : int { Coil = 0, DiscreteInput = 1, InputRegister = 2, HoldingRegister = 3 };
enum class DataType : int { Bool = 0, Int16 = 1, UInt16 = 2, Int32 = 3, UInt32 = 4, Float32 = 5, BitField = 6 };
enum class ByteOrder : int { ABCD = 0, DCBA = 1, BADC = 2, CDAB = 3 };
enum class WordOrder : int { HighLow = 0, LowHigh = 1 };
enum class HistoryMode : int { Periodic = 0, OnChange = 1 };

struct Tag {
    int id = -1;
    QString name;
    RegisterType registerType = RegisterType::HoldingRegister;
    int address = 0;              // 零基 PDU 地址
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

    int registerCount() const;   // Bool/Int16/UInt16→1; Int32/UInt32/Float32→2
    int modbusAddress() const;   // 返回零基 address
};

// TagValue.h —— 运行时值 + 质量
enum class Quality : int { Good = 0, Stale = 1, Bad = 2, Disconnected = 3 };

struct TagValue {
    int tagId = -1;
    QVariant value;       // 工程值（已应用 scale/offset）
    QVariant rawValue;    // 原始寄存器值
    Quality quality = Quality::Disconnected;
    QDateTime timestamp;
    QString error;
};
```

质量状态语义（冻结）：

| 质量 | 含义 | 触发/恢复 |
|---|---|---|
| `Good` | 最近一次读成功且未超时 | 收到有效回复 |
| `Stale` | 单次请求失败或回复超时（质量降级但未断线） | 单次失败标记；下次成功恢复 Good |
| `Bad` | 连续失败（阈值见 AcquisitionEngine 规则） | 连续 N 次失败（N≥3） |
| `Disconnected` | 未连接或已断开 | 连接断开；重连成功后恢复 Good |

---

## 2. IModbusClient（抽象基类）

- 头文件：`src/modbus/IModbusClient.h`
- 线程：**通信线程**（对象创建于通信线程，不得跨线程直接调用）
- 实现：`QtModbusClient`（包装 `QModbusTcpClient`，可替换为 libmodbus 而不影响上层）

```cpp
class IModbusClient : public QObject {
    Q_OBJECT
public:
    explicit IModbusClient(QObject* parent = nullptr);
    virtual ~IModbusClient() = default;

    // 建立连接：host/port/unitId。连接参数变更必须断开后重新连接。
    virtual void connectToDevice(const QString& host, int port, int unitId) = 0;
    // 主动断开。手动断开后 AcquisitionEngine 不得自动重连。
    virtual void disconnectFromDevice() = 0;
    virtual bool isConnected() const = 0;

    // 异步请求。返回的 QModbusReply 归调用方所有，必须 connect(finished) 后 deleteLater。
    // 同一时间最多一个在途请求（由 AcquisitionEngine 保证，本接口不强制）。
    virtual QModbusReply* sendReadRequest(const QModbusDataUnit& unit, int serverAddress) = 0;
    virtual QModbusReply* sendWriteRequest(const QModbusDataUnit& unit, int serverAddress) = 0;

    virtual void setTimeout(int ms) = 0;
    virtual void setNumberOfRetries(int n) = 0;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
};
```

语义与约束：

- `serverAddress` 使用 **零基** 地址（与 `QModbusDataUnit` 的 `startAddress` 一致）。
- `connectToDevice` 失败（超时/拒绝）通过 `errorOccurred` 上报；成功/断开通过 `connected`/`disconnected`。
- 超时与重试：`setTimeout(ms)` 默认 1000ms；`setNumberOfRetries` 默认 2。重试发生在 QModbusClient 内部，`errorOccurred` 只在最终失败时发出。
- `QModbusReply` 生命周期：请求发出后，若 `AcquisitionEngine` 停止或连接断开，必须保证 reply 被 `abort()` 并 `deleteLater()`，不得泄漏。
- **错误路径：** 未连接时调用 sendReadRequest/sendWriteRequest 返回 `nullptr`；调用方必须判空。

---

## 3. ValueCodec（无状态工具类）

- 头文件：`src/modbus/ValueCodec.h`
- 线程：任意（纯静态、无成员状态、无锁）

```cpp
// 解码结果
struct DecodeResult {
    QVariant value;      // 工程值（apply scale/offset 后）
    bool valid = false;
    QString error;       // 失败原因（单位不足、地址越界、类型非法等）
};

class ValueCodec {
public:
    // 从读回复数据单元中按 tag 定义解码一个值
    static DecodeResult decode(const QModbusDataUnit& unit, const Tag& tag);
    // 将用户输入值按 tag 定义编码为写数据单元（起始地址 = tag 零基地址）
    static QModbusDataUnit encode(const Tag& tag, const QVariant& value);
    // 返回 tag 的零基 PDU 起始地址（恒等于 tag.address）
    static int modbusAddress(const Tag& tag);
};
```

解码规则（冻结）：

| DataType | 寄存器数 | 说明 |
|---|---|---|
| Bool | 1 | 线圈/离散输入/寄存器 bit0（寄存器场景取 bitPosition） |
| Int16/UInt16 | 1 | 单寄存器 |
| Int32/UInt32/Float32 | 2 | 双寄存器，按 byteOrder（ABCD/DCBA/BADC/CDAB）+ wordOrder（HighLow/LowHigh）组合 |

- `BitField`：在 `bitPosition` 处提取 `bitLength` 位。
- `decode` 错误路径（返回 `valid=false`）：`unit` 寄存器数量不足、`unit.startAddress` 与 tag 地址不匹配（越界）、未知 `DataType`、寄存器值不合法。
- `encode` 错误路径：调用方（UI 层）应在编码前做类型/范围校验，`encode` 对非法输入返回起始地址正确的空数据单元并记录 error（通过 `qWarning`，不抛异常）。

---

## 4. PollPlanner

- 头文件：`src/modbus/PollPlanner.h`
- 线程：通信线程（也可在测试中任意线程使用；纯函数式，无内部状态）

```cpp
// 一个轮询组 = 一次 Modbus 读请求覆盖的连续寄存器块
struct PollGroup {
    RegisterType registerType;
    int startAddress;      // 零基起始地址
    int count;             // 寄存器数（≤125）
    int intervalMs;        // 本组轮询周期
    QList<int> tagIds;     // 本组覆盖的 tag id（按地址升序）
};

class PollPlanner {
public:
    // 由全部 tag 列表生成轮询组。规则见下。
    static QList<PollGroup> buildGroups(const QList<Tag>& tags);
};
```

分组规则（冻结，Phase 1 CORE-04）：

1. 按 `registerType` 分组；不同功能区（线圈/离散/输入/保持）绝不合并。
2. 组内按 `address` 升序排序。
3. 相邻 tag 地址间隔 ≤ 5 个寄存器时合并为一个读取块；间隔 > 5 拆块。
4. 单块寄存器数上限 **125**（Modbus PDU 上限），超限必须拆分。
5. 按 `pollIntervalMs` 分组：同一块内所有 tag 周期一致；周期不同必须拆开（200/500/1000/2000/5000ms，默认 500ms）。
6. 空输入返回空列表。

---

## 5. AcquisitionEngine（通信线程 QObject）

- 头文件：`src/runtime/AcquisitionEngine.h`
- 线程：**通信线程**（构造后 `moveToThread`，或直接在通信线程构造）

```cpp
class AcquisitionEngine : public QObject {
    Q_OBJECT
public:
    AcquisitionEngine(IModbusClient* client, TagCache* cache,
                      QList<Tag> tags, QObject* parent = nullptr);

public slots:   // 均可经 QueuedConnection / invokeMethod 从其他线程调用
    void start();                       // 建 PollGroup、开始轮询、发起连接
    void stop();                        // 停止轮询、断开连接、清空写队列
    void enqueueWrite(const WriteCommand& cmd);
    void cancelPendingWrites();         // 断开/停止时调用，丢弃所有待写命令

signals:
    void tagValuesUpdated(const QHash<int, TagValue>& values);   // 每轮询周期合并后发出
    void connectionStateChanged(ConnectionState state);
    void writeCompleted(int tagId, bool success, const QString& error);
};
```

行为规则（冻结，Phase 1 CORE-08）：

- **构造注入** `IModbusClient*`、`TagCache*`、`QList<Tag>`；引擎不拥有 client，拥有 tags 拷贝。
- **在途请求上限 = 1**：任何时刻至多一个未完成读请求；回复处理完毕（或超时）后才发下一个。写请求插在两个轮询间隙执行。
- **轮询调度**：按 PollGroup.intervalMs 定时发读请求；回复到达 → `ValueCodec::decode` → `TagCache::updateValues` → `emit tagValuesUpdated`（合并为一次 QHash）。
- **质量转移**（冻结）：
  - 单次失败（超时/错误）→ 该组 tag 标记 `Stale`；下一次成功恢复 `Good`。
  - 同一组连续 3 次失败 → 标记 `Bad`。
  - 连接断开 → 全部 tag 标记 `Disconnected`。
- **重连退避**（冻结）：连接断开后自动重连，退避序列 `1s/2s/4s/8s/16s/30s`（30s 封顶，保持重试）。**手动断开（UI 断开按钮）不自动重连**。
- **写流程**：`enqueueWrite` → WriteQueue → 轮询间隙 `dequeue` → `sendWriteRequest` → reply 完成 → `emit writeCompleted(tagId, success, error)`。写失败不改变 tag 质量（由读路径决定）。
- **stop()**：取消在途 reply（abort + deleteLater）、断开连接、`cancelPendingWrites()`。
- `connectionStateChanged` 与 `ConnectionState` 枚举（见 §8）。

---

## 6. TagCache（线程安全值缓存）

- 头文件：`src/runtime/TagCache.h`
- 线程：**跨线程共享**（通信线程写、UI 线程读）。非 QObject，内部用 `QMutex` 保护。

```cpp
class TagCache {
public:
    explicit TagCache(int capacityHint = 1024);

    void updateValues(const QHash<int, TagValue>& values);   // 合并更新，不丢失未更新的 tag
    QHash<int, TagValue> snapshot() const;                   // 全量拷贝（加锁）
    TagValue value(int tagId) const;                         // 不存在返回默认构造（quality=Disconnected）
    QList<int> staleTagIds(int thresholdMs) const;           // timestamp 早于 (now-thresholdMs) 的 tag

private:
    mutable QMutex m_mutex;
    QHash<int, TagValue> m_values;
};
```

规则（冻结，Phase 1 CORE-06）：

- `updateValues` 与 `snapshot`/`value`/`staleTagIds` 全部互斥；快照返回**值拷贝**，调用方持有后与缓存无关。
- `staleTagIds` 用于 UI 显示与报警判定：`now - value.timestamp > thresholdMs` 即视为过期。
- 无 tag 值 → `value()` 返回默认 `TagValue{tagId, {}, {}, Disconnected, QDateTime(), {}}`。

---

## 7. WriteQueue

- 头文件：`src/runtime/WriteQueue.h`
- 线程：**通信线程**（由 AcquisitionEngine 独占访问，内部不加锁；测试可在任意单线程中使用）

```cpp
struct WriteCommand {
    QUuid id = QUuid::createUuid();   // 唯一标识（点动释放与按下通过同 id 关联）
    int tagId = -1;
    QVariant value;                    // 目标值（工程值，编码在引擎内完成）
    QDateTime createdAt = QDateTime::currentDateTime();
    int expiryMs = 5000;               // 存活时长，默认 5s
    bool isRelease = false;            // 点动释放标志
    int priority = 0;                  // 0=普通, 1=高（点动释放强制置 1）
};

class WriteQueue {
public:
    void enqueue(WriteCommand cmd);
    void clear();                                   // 丢弃全部待处理命令
    std::optional<WriteCommand> dequeue();          // 按优先级+FIFO 出队；空则 nullopt
    void removeExpired(int nowMs);                  // 丢弃 createdAt+expiryMs < nowMs 的命令
    bool isEmpty() const;
};
```

规则（冻结，Phase 1 CORE-07）：

- **优先级**：`priority=1`（点动释放）总是先于 `priority=0` 出队；同优先级同时刻按入队顺序 FIFO。
- **过期**：`removeExpired` 由引擎在每次写循环前调用，丢弃过期命令（防旧命令重放/迟到释放）。
- **点动释放**：`isRelease=true` 的命令由 `ButtonActionExecutor` 构造时**强制 priority=1**（在 executor 侧完成，Queue 不自动改写）。
- `clear()` 在 `stop()`/`cancelPendingWrites()`/手动断开时调用。

---

## 8. ConnectionState / 按钮模型

```cpp
// src/runtime/AcquisitionEngine.h 或独立头文件
enum class ConnectionState : int {
    Disconnected = 0,   // 未连接（初始态）
    Connecting = 1,     // 正在建立连接/重连
    Online = 2,         // 在线且轮询正常
    Degraded = 3,       // 在线但有连续失败（质量 Bad 或部分 Stale 且重试中）
    Reconnecting = 4    // 掉线后按退避自动重连中（区别于手动断开）
};
```

| 状态 | 触发 | UI 表现 |
|---|---|---|
| Disconnected | 初始 / 手动断开 | 灰 |
| Connecting | 发起连接 / 首次重连 | 黄闪烁 |
| Online | 连接成功且轮询回复正常 | 绿 |
| Degraded | 连续失败但未断开（重试中） | 黄 |
| Reconnecting | 掉线后自动重连退避中 | 黄闪烁 |

状态转移（冻结）：`Disconnected → Connecting → Online ↔ Degraded → Reconnecting → Connecting → Online`；手动断开 → 直接 `Disconnected`（不进入 Reconnecting）。

---

## 9. 看板模型（DashboardDocument）

头文件：`src/dashboard/DashboardDocument.h`

```cpp
struct DashboardPage {
    int id = -1;              // -1 = 未保存（新页）
    QString name;
    int width = 1920;
    int height = 1080;
    QString background;       // 颜色（#RRGGBB）或图片资源路径；空 = 默认
    int sortOrder = 0;        // 页面排序
};

struct DashboardItem {
    int id = -1;
    int pageId = -1;
    QString itemType;         // text/rect/image/value/led/switch/progress/gauge/trend/button/errorPlaceholder
    qreal x = 0, y = 0;
    qreal width = 100, height = 100;
    qreal zOrder = 0;
    QJsonObject commonStyle;  // fillColor, borderColor, borderWidth, font, radius...
    QJsonObject config;       // tagId, min, max, unit, action...（组件相关）
    int schemaVersion = 1;    // config JSON 结构版本
};
```

- 组件类型清单（冻结，Phase 2）：`text, rect, image, value, led, switch, progress, gauge, trend, button`；损坏组件的反序列化失败统一降级为 `errorPlaceholder`（黄色占位框，不阻塞其他组件/页面）。
- `config` 必须可独立反序列化；未知类型/结构损坏时按占位处理。
- `commonStyle` 是 UI 表现层属性，不含业务绑定；`config` 含业务绑定（tagId 等）。

---

## 10. DashboardRepository

- 头文件：`src/dashboard/DashboardRepository.h`
- 线程：**数据库线程**（持有 DB 连接名；跨线程调用必须经信号/槽或 invokeMethod 转到数据库线程执行）

```cpp
class DashboardRepository {
public:
    explicit DashboardRepository(const QString& connectionName);   // 指向数据库线程的命名连接

    QList<DashboardPage> loadPages();
    bool savePage(DashboardPage& page);     // id=-1 时 INSERT 并回填 id
    bool deletePage(int pageId);            // 级联删除该页 items（DB 外键 CASCADE）
    QList<DashboardItem> loadItems(int pageId);
    bool saveItems(int pageId, const QList<DashboardItem>& items);  // 事务内 DELETE+INSERT
};
```

规则（冻结，Phase 2 DASH-01）：

- 所有方法**同步阻塞**（调用方已在数据库线程）；UI 侧如需同步等待，经 `QMetaObject::invokeMethod(..., Qt::BlockingQueuedConnection)`，或在控制器层用异步回调。
- `saveItems` 在**一个事务**内完成 DELETE + 批量 INSERT，失败整体回滚并返回 false。
- 写入前校验：pageId 有效、itemType 已知、几何非负、config 可解析（Phase 2 DASH-10）。

---

## 11. 按钮动作模型与执行器

头文件：`src/dashboard/runtime/ButtonAction.h`、`src/dashboard/runtime/ButtonActionExecutor.h`

```cpp
enum class ButtonActionType : int {
    Momentary = 0,      // 点动：按下写 paramA，释放写 paramB
    Toggle = 1,         // 切换：每次按下翻转当前值（paramA=ON值, paramB=OFF值）
    FixedValue = 2,     // 固定值：按下写 paramA
    InputValue = 3,     // 弹窗输入：输入校验后写入
    NavigatePage = 4    // 页面跳转
};

struct ButtonAction {
    ButtonActionType type = ButtonActionType::FixedValue;
    QVariant paramA;         // Momentary: 按下值 / FixedValue: 目标值 / Toggle: ON 值
    QVariant paramB;         // Momentary: 释放值（默认 = 按下前的值） / Toggle: OFF 值
    int targetPageId = -1;   // NavigatePage 目标页
    QString confirmMessage;  // 非空则先弹确认框
};
```

```cpp
class ButtonActionExecutor : public QObject {
    Q_OBJECT
public:
    explicit ButtonActionExecutor(const TagCache* cache, const QList<Tag>& tags,
                                  QObject* parent = nullptr);

    void execute(const ButtonAction& action, int tagId);   // 触发动作
    void releaseMomentary(int tagId);                      // 点动释放（isRelease=true, priority=1）
    void releaseAllMomentary();                            // 释放所有点动（页面/模式切换、失焦时）
    bool isButtonEnabled(int tagId) const;                 // 按钮可用性判定

signals:
    void writeRequested(const WriteCommand& cmd);          // → UI 转发到通信线程引擎
    void pageNavigationRequested(int pageId);              // → DashboardController 切换页面
};
```

规则（冻结，Phase 2 DASH-08）：

- **线程**：UI 主线程。`execute` 内同步构造 `WriteCommand` 并 `emit writeRequested`；转发由 UI 层连接到引擎（QueuedConnection）完成。
- **isButtonEnabled 判定**（全部满足才可用）：
  1. 连接状态非 Disconnected/Reconnecting；
  2. Tag 非 `readOnly`；
  3. `TagCache::value(tagId).quality` 非 Bad/Disconnected；
  4. FixedValue/InputValue/Toggle 类型与范围校验通过。
- **点动安全**：Momentary 按下后启动 `QTimer::singleShot(3000)` 超时自动释放（最大保持 3s）；窗口失焦、页面切换、退出运行模式时 `releaseAllMomentary()`。
- **释放命令**：`releaseMomentary` 构造 `isRelease=true, priority=1` 的 WriteCommand（保持与按下命令同 tagId；`value` 取 paramB 或按下前值）。
- **确认弹窗**：`confirmMessage` 非空时 `QMessageBox::question`，取消则不执行。
- 编辑模式（editMode=true）下 **严禁** 产生任何写命令：executor 在编辑模式被禁用，UI 层不再调用 execute。

---

## 12. 跨接口依赖图（数据流）

```
[UI 线程]                      [通信线程]                     [数据库线程]
ButtonActionExecutor ──writeRequested──▶ AcquisitionEngine::enqueueWrite
TagCache(snapshot) ◀──tagValuesUpdated─ AcquisitionEngine ──TagCache.updateValues
TagMonitor/看板 UI ◀──connectionStateChanged
MainWindow ──invokeMethod──▶ engine.start/stop
AlarmEngine(通信线程) ──alarmEvent──▶ 数据库线程写入 alarm_events
UI 配置/看板/配方 CRUD ──invokeMethod──▶ DatabaseMigrator/Repository（db_thread 连接）
```

错误路径汇总：

| 场景 | 处理 |
|---|---|
| 未连接发请求 | 返回 nullptr，调用方判空并记日志 |
| 请求超时 | IModbusClient 重试（默认 2 次）后 errorOccurred；引擎标记 Stale |
| 解码失败 | 该 tag 标记 Bad + error 文本；不影响其他 tag |
| 写命令过期 | removeExpired 丢弃；不发送 |
| 写失败 | writeCompleted(tagId, false, error)；质量由读路径决定 |
| 组件 config 损坏 | errorPlaceholder 占位，页面其余部分正常加载 |
| 手动断开 | 不自动重连；写队列清空；tag 全部 Disconnected |

---

## 13. 变更管理

- 本文件冻结后，任何接口签名、信号参数、枚举值、线程归属的变更，需先在计划文档中更新对应任务，再同步修改本文件，最后通知 @reviewer 复核。
- 未冻结的扩展点（后续阶段可选）：字符串类型 Tag（低优先级）、批量写（当前仅单写）、MQTT/OPC UA 桥接（不纳入当前版本）。
