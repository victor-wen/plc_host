# Qt Serial Bus：Modbus TCP

> Qt 版本：6.8（官方页面当前显示的补丁版本为 6.8.8）
> 拉取日期：2026-08-06
> 来源：<https://doc.qt.io/qt-6.8/qmodbustcpclient.html>、<https://doc.qt.io/qt-6.8/qmodbusclient.html>、<https://doc.qt.io/qt-6.8/qmodbusdevice.html>、<https://doc.qt.io/qt-6.8/qmodbusdataunit.html>、<https://doc.qt.io/qt-6.8/qmodbusreply.html>、<https://doc.qt.io/qt-6.8/qtserialbus-modbus-client-example.html>、<https://doc.qt.io/qt-6.8/threads-qobject.html>
> Context7 libraryId：`/websites/doc_qt_io_qt-6`

## 模块和对象职责

`QModbusTcpClient` 是 Modbus TCP 客户端接口，继承自 `QModbusClient`；后者继承自 `QModbusDevice`。模块和链接方式：

```cpp
find_package(Qt6 REQUIRED COMPONENTS SerialBus)
target_link_libraries(mytarget PRIVATE Qt6::SerialBus)
```

`QModbusClient` 负责请求排队、超时/重试配置和异步请求；`QModbusReply` 表示某一个请求的结果；`QModbusDataUnit` 描述连续的寄存器区间和数据。Qt 文档指出一个 `QModbusClient` 通常足够整个应用使用，但实际并行请求数由协议实现决定。

## QModbusTcpClient 连接

### 构造和参数

```cpp
explicit QModbusTcpClient(QObject *parent = nullptr);
```

TCP 连接使用 `QModbusDevice::setConnectionParameter()` 设置：

```cpp
auto *client = new QModbusTcpClient;
client->setConnectionParameter(
    QModbusDevice::NetworkAddressParameter, QStringLiteral("192.0.2.10"));
client->setConnectionParameter(
    QModbusDevice::NetworkPortParameter, 502);
client->setTimeout(1000);
client->setNumberOfRetries(3);
```

`NetworkAddressParameter` 的类型是 `QString`，`NetworkPortParameter` 的类型是 `int`。默认网络地址是 localhost，默认端口是 `502`。连接已经建立后改变连接参数不会影响当前连接；要使新参数生效，应先断开再连接。

### 打开、关闭和状态

对外使用继承的 `connectDevice()` 和 `disconnectDevice()`；`QModbusTcpClient::open()`、`close()` 是受保护的重实现，不应由业务代码直接调用。

```cpp
connect(client, &QModbusDevice::stateChanged,
        this, &Controller::onModbusStateChanged);
connect(client, &QModbusDevice::errorOccurred,
        this, &Controller::onModbusError);

if (!client->connectDevice()) {
    qWarning() << client->errorString();
}

// 关闭时：
client->disconnectDevice();
```

`connectDevice()` 返回 `true` 只表示连接过程已成功启动；最终是否连接成功要以 `stateChanged(QModbusDevice::ConnectedState)` 为准。状态枚举为：

| 状态 | 含义 |
|---|---|
| `UnconnectedState` | 未连接 |
| `ConnectingState` | 正在连接 |
| `ConnectedState` | 已连接到 Modbus 网络 |
| `ClosingState` | 正在关闭 |

信号签名：

```cpp
void stateChanged(QModbusDevice::State state);
void errorOccurred(QModbusDevice::Error error);
```

## 读写请求

```cpp
QModbusReply *sendReadRequest(const QModbusDataUnit &read, int serverAddress);
QModbusReply *sendWriteRequest(const QModbusDataUnit &write, int serverAddress);
QModbusReply *sendReadWriteRequest(const QModbusDataUnit &read,
                                   const QModbusDataUnit &write,
                                   int serverAddress);
```

请求是异步的。返回非空指针表示请求已被接受；参数非法或无法排队时立即返回 `nullptr`。`serverAddress` 是目标从站地址；Modbus TCP 的实际使用仍应遵循设备对 Unit ID 的约定。

`sendReadWriteRequest()` 使用读写多个保持寄存器功能码，`read` 和 `write` 都必须是 `HoldingRegisters` 类型。业务层应检查空指针，并且只通过 `QModbusReply::finished()` 或 `errorOccurred()` 处理完成结果。

## QModbusDataUnit

### RegisterType

```cpp
enum RegisterType {
    Invalid,
    DiscreteInputs,
    Coils,
    InputRegisters,
    HoldingRegisters
};
```

`DiscreteInputs` 和 `InputRegisters` 通常是输入/只读区，`Coils` 和 `HoldingRegisters` 可由应用写入，但最终读写权限取决于设备。值统一以单 bit 或 `quint16` 表示；线圈和离散输入中 `0` 被视为 0，非 0 被视为 1。

常用构造函数和访问器：

```cpp
QModbusDataUnit();
explicit QModbusDataUnit(RegisterType type);
QModbusDataUnit(RegisterType type, int address,
                const QList<quint16> &data);
QModbusDataUnit(RegisterType type, int address, quint16 size);

bool isValid() const;
RegisterType registerType() const;
void setRegisterType(RegisterType type);
int startAddress() const;
void setStartAddress(int address);
qsizetype valueCount() const;
void setValueCount(qsizetype newCount);
QList<quint16> values() const;
void setValues(const QList<quint16> &values);
quint16 value(qsizetype index) const;
void setValue(qsizetype index, quint16 value);
```

默认构造的单元无效，起始地址为 `-1`。有效单元要求 `registerType() != Invalid` 且 `startAddress() >= 0`。读请求通常使用 `(type, address, size)` 构造，此时 `valueCount()` 表示请求数量，`values()` 可能为空；请求完成后，`valueCount()` 才与返回值数量对应。地址是 PDU 地址，项目数据库保存零基地址；界面的一基地址换算必须在项目代码中显式完成。

## QModbusReply 生命周期和信号

```cpp
enum ReplyType {
    Raw,
    Common,
    Broadcast
};

void finished();
void errorOccurred(QModbusDevice::Error error);

bool isFinished() const;
QModbusDevice::Error error() const;
QString errorString() const;
QModbusDataUnit result() const;
QModbusResponse rawResult() const;
QList<QModbusDevice::IntermediateError> intermediateErrors() const;
int serverAddress() const;
ReplyType type() const;
```

- `finished()` 表示处理结束，但不代表一定成功；应同时检查 `error()`。
- 结束后回复数据不再更新。读请求成功时 `result()` 含服务器返回值；写请求、失败、尚未结束和广播回复的 `result()` 无效。协议错误时可用 `rawResult()` 查看原始异常响应。
- 广播回复的服务器地址为 `0`，`finished()` 会立即发出，且 `result()` 始终无效。
- 在 `finished()` 或 `errorOccurred()` 槽中不要直接 `delete reply`，使用 `reply->deleteLater()`。错误信号之后通常还会跟随 `finished()`，避免重复清理。

```cpp
auto *reply = client->sendReadRequest(unit, serverAddress);
if (!reply) {
    emit readFailed(QStringLiteral("request rejected"));
    return;
}

connect(reply, &QModbusReply::errorOccurred,
        this, [reply](QModbusDevice::Error error) {
    qWarning() << error << reply->errorString();
});
connect(reply, &QModbusReply::finished, this, [this, reply] {
    if (reply->error() == QModbusDevice::NoError) {
        consume(reply->result());
    }
    reply->deleteLater();
});
```

## 超时、重试和错误码

```cpp
int timeout() const;
void setTimeout(int newTimeout);
int numberOfRetries() const;
void setNumberOfRetries(int number);
void timeoutChanged(int newTimeout);
```

`setTimeout()` 的最小值为 10 ms，默认值为 1000 ms；当前已运行的计时器不受修改影响。`setNumberOfRetries()` 要求参数大于等于 0，默认值为 3，修改只影响新请求，不影响已经排队的请求。

### `QModbusDevice::Error`

| 枚举 | 含义 |
|---|---|
| `NoError` | 没有错误 |
| `ReadError` | 读操作错误 |
| `WriteError` | 写操作错误 |
| `ConnectionError` | 打开后端/连接错误 |
| `ConfigurationError` | 设置连接参数错误 |
| `TimeoutError` | I/O 在给定时限内未完成 |
| `ProtocolError` | Modbus 协议错误；可检查原始响应中的异常码 |
| `ReplyAbortedError` | 设备断开导致回复被中止 |
| `UnknownError` | 未知错误 |
| `InvalidResponseError`（Qt 6.4 起） | 解析响应失败，或当前实现不支持该功能码 |

### 中间错误和重发

```cpp
enum IntermediateError {
    ResponseCrcError,
    ResponseRequestMismatch
};
```

中间错误表示一次完整收发周期中的 CRC 错误或响应与请求不匹配。Qt 文档说明这类帧可能在达到最大重试次数前被重新发送，可通过 `intermediateErrors()` 诊断。它们不是最终的 `Error` 值；最终结果仍以 `error()` 和 `finished()` 为准。

## 并发、连接保活和项目策略

Qt 会为客户端排队请求，但并行执行数量依协议实现而定，官方 API 没有给 `QModbusTcpClient` 提供一个统一的“最大在途请求数”设置。为兼容 PLC 和便于把回复与采集批次对应，项目约束是**同一客户端最多一个在途请求**：收到 `finished()`/最终错误后再发送下一个；不要在定时器中无条件叠加请求。

`QModbusTcpClient` 官方类页没有承诺应用层心跳或自动 keep-alive，也没有为 TCP keep-alive 提供专门的 `QModbusTcpClient` 配置项。虽然 `QModbusDevice::device()` 可以返回底层 `QIODevice *`，官方同时提醒不要保存该指针，因为它可能随时失效；项目不依赖它配置保活。`setTimeout()` 和重试只服务于请求收发，不能当作 TCP 心跳。项目若需要检测链路，应在通信线程中按业务周期发起一个合法的、无副作用的读请求，并结合状态/错误信号处理；不要通过跨线程直接访问底层 socket 实现“保活”。断开会使未完成的回复得到 `ReplyAbortedError`，重连后不自动重放已经失败的业务写入。

## 线程安全约束

- `QModbusTcpClient`、它产生的 `QModbusReply`、定时器和采集调度器在通信线程创建并使用；该线程必须有事件循环。
- `QObject` 的线程亲和性决定事件和 queued slot 的执行线程。不能在 UI 线程直接调用通信线程对象的连接、读写或配置函数。
- UI 到通信线程只使用信号/槽，必要时使用 `Qt::QueuedConnection` 或 `QMetaObject::invokeMethod()`；通信结果通过信号携带值类型数据回 UI。
- `QWidget` 只在主线程使用。跨线程不要传递仍由通信对象拥有的回复指针；处理完回复后在其所属线程 `deleteLater()`。
