# Qt 6.8 本地参考

> Qt 版本：6.8（官方页面当前显示的补丁版本为 6.8.8）
> 拉取日期：2026-08-06
> 主要资料源：Qt 官方文档站 <https://doc.qt.io/qt-6.8/>
> Context7 libraryId：`/websites/doc_qt_io_qt-6`

## 用途

本目录只保存 `plc_host` 实现所需的 Qt API、行为和约束摘要，不是 Qt 网站的离线镜像。类的完整成员列表、平台差异、许可条款和补丁版本变化，以对应的 Qt 6.8 官方页面为准。

代码片段中的类名、函数名、枚举名和 C++ 签名保持英文；说明文字使用中文。除非文档明确标为“项目约束”，不要把本目录的建议误认为 Qt API 的默认行为。

## 文档索引

| 文档 | 覆盖范围 |
|---|---|
| [qt-modbus-tcp.md](qt-modbus-tcp.md) | `QModbusTcpClient`、数据单元、异步回复、错误和通信线程 |
| [qt-widgets-graphics-view.md](qt-widgets-graphics-view.md) | `QGraphicsScene`、`QGraphicsView`、`QGraphicsObject` 和自定义项绘制 |
| [qt-undo-framework.md](qt-undo-framework.md) | `QUndoStack`、`QUndoCommand`、命令合并和宏命令 |
| [qt-sqlite-threading.md](qt-sqlite-threading.md) | `QSqlDatabase` 线程模型、SQLite WAL、锁等待和迁移 |
| [qt-test-framework.md](qt-test-framework.md) | Qt Test 断言、信号侦听、异步等待、CMake 和 CTest |
| [qt-charts.md](qt-charts.md) | Qt Charts 的图表、曲线、坐标轴和实时更新 |
| [qt-windows-deployment.md](qt-windows-deployment.md) | `windeployqt`、CMake 安装部署和依赖检查 |

## 项目统一约束

1. **只用 Qt 6.8 API。** 不从 Qt 5 文档复制签名，也不把 Qt Graphs 的类当成 Qt Charts 的类。当前项目技术基线为 Qt 6.8 LTS、C++20、CMake 3.24+、MSVC 2022 x64。
2. **遵守 QObject 线程亲和性。** `QObject` 及其事件驱动子类应在创建它的线程中使用；对象有父对象时不能随意 `moveToThread()`。`QWidget`、Graphics View、Qt Charts 和撤销栈属于 UI 主线程。
3. **跨线程只走消息边界。** 工作线程和 UI/数据库线程之间只使用信号/槽（需要时显式 `Qt::QueuedConnection`）或 `QMetaObject::invokeMethod()`；不得从另一线程直接调用一个正在接收事件的 `QObject`。
4. **数据库连接按线程隔离。** 每个线程创建并使用自己的命名 `QSqlDatabase` 连接；不要在线程间共享连接、`QSqlQuery` 或活动结果集。
5. **先看本地摘要，再回官方页。** 当代码需要未列出的成员、平台选项或行为时，直接打开文档顶部列出的官方 URL，不要凭记忆补 API。

## 线程安全约束

所有 QObject 遵循线程亲和性：对象及其事件驱动子类在创建/归属线程中使用，父对象和子对象不能跨线程随意组合。`QWidget`、Graphics View、Qt Charts 和撤销栈只在 UI 主线程；`QModbusTcpClient` 在通信线程；`QSqlDatabase` 连接按线程独立创建。跨线程仅用信号/槽（必要时 `Qt::QueuedConnection`）或 `QMetaObject::invokeMethod()`，不直接调用另一个线程中正在接收事件的 QObject。

## 许可和引用

这些文件是面向本项目的事实摘要，引用 Qt API 时保留官方链接。Qt 文档本身的许可和版权声明见各官方页面底部；本目录不重新发布 Qt 文档全文或图片。
