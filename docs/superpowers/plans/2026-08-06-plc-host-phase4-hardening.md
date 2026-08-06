# Phase 4: 稳定性与发布

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 压力测试、故障注入、实机验收、Windows 发布包。

**Tech Stack:** C++20, Qt 6.8, Qt Test, CTest, windeployqt, CMake

## Global Constraints

- 1000 Tag + 100 画布组件 8 小时无崩溃、无死锁、无误写。
- Windows 发布包在无 Qt 的干净机器上可运行。
- @debugger 只读定位，@coder 执行修复。
- @frontend (Luna) 做最终视觉一致性审查。

---

### Task HARD-01: 本机 Modbus TCP 模拟服务器

**Agent:** `@coder`
**Depends on:** CORE-05

**Files to create:**
- `tests/mock/MockModbusServer.h/cpp`
- `tests/integration/tst_MockServer.cpp`

**MockModbusServer (基于 QModbusTcpServer 或 QTcpServer):**
- 模拟线圈、离散输入、输入寄存器、保持寄存器四个区
- 可注入特定功能码的异常响应
- 可模拟超时 (延迟回复或不应答)
- 可模拟连接断开
- 记录所有收到的写请求供测试断言

**tst_MockServer.cpp:**
- QtModbusClient 连接 mock 服务器
- 读寄存器返回预设值
- 写线圈后值变化
- 异常码正确处理
- 超时后 AcquisitionEngine 标记 Stale
- 断开后标记 Disconnected 并重连

**Commit:** `test: MockModbusServer for integration testing of all Modbus scenarios`

---

### Task HARD-02: 故障注入集成测试

**Agent:** `@coder`
**Depends on:** HARD-01, CORE-08

**Files to create:**
- `tests/integration/tst_FaultInjection.cpp`

**测试场景:**
1. 正常轮询 100 个 Tag，验证全部 Good
2. 注入一次超时 → Stale → 下次成功 → Good 恢复
3. 连续 3 次超时 → Bad → Disconnected
4. 断开重连 → Connecting → Online → 恢复轮询
5. 写入时断开 → writeCompleted(success=false) → 写队列清空
6. IllegalDataAddress 异常 → error 信息正确 → 对应 Tag 标记 Bad
7. ServerDeviceFailure 异常 → 重试次数耗尽 → Disconnected
8. 退避时间: 验证 1/2/4/8/16/30s 间隔

**Commit:** `test: fault injection integration tests for timeout, disconnect, exceptions, and recovery`

---

### Task HARD-03: 压力测试

**Agent:** `@coder`
**Depends on:** HARD-01, DASH-09, CORE-08

**Files to create:**
- `tests/stress/tst_StressTest.cpp`

**压力测试脚本:**
1. 创建 1000 个 Tag (混合类型，200ms-5000ms 采集周期)
2. 创建 100 个画布组件 (所有类型，分布多页)
3. MockModbusServer 响应延迟 10-50ms
4. 循环: 读 Tag 值 → 随机写值 → 切换看板页面 → 切换编辑/运行模式
5. 持续 30 分钟自动化 (CI 用) 或手动 8 小时

**验证指标:**
- 无崩溃
- QModbusTcpClient 未跨线程操作 (QObject::thread() 断言)
- 编辑模式写请求 count=0
- 内存增长 < 100MB (30 分钟)
- TagCache snapshot 始终包含 1000 个条目
- 数据库 history_samples 持续增长

**Commit:** `test: 1000-tag 100-item stress test with mock server for 30min CI`

---

### Task HARD-04: 故障排查 (按需)

**Agent:** `@debugger`
**Trigger:** 以下任一情况:
- 同一测试连续修复两次仍失败
- 偶发失败或只在全量测试中失败
- Qt "Cannot move to target thread" 等跨线程警告
- 数据库锁、死锁、关闭崩溃
- Modbus 回复与请求错配
- 点动按钮释放动作偶发丢失
- 开发机与部署机行为不同

**流程:**
1. @debugger 只读分析: 列事实 → 建假设 → 验证 → 定位根因
2. 输出最小修复建议
3. 交给 @coder 实现
4. @reviewer 复审

---

### Task HARD-05: 修复调试发现的问题

**Agent:** `@coder`
**Depends on:** HARD-04

根据 @debugger 的修复建议逐一实施，每个修复后运行全量 ctest。
如修复引入新问题，重新触发 HARD-04。

---

### Task HARD-06: Windows 部署与安装

**Agent:** `@coder`
**Depends on:** 全部编码任务完成

**Files to modify:**
- `CMakeLists.txt` (添加 install 规则)

**CMake 安装规则:**
```cmake
install(TARGETS plc_host RUNTIME DESTINATION bin)
qt_generate_deploy_app_script(
    TARGET plc_host
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
```

**发布验证步骤:**
```powershell
cmake --build build --config Release
cmake --install build --prefix dist
& "$env:QT_ROOT\bin\windeployqt.exe" --dir dist/bin dist/bin/plc_host.exe
# 在干净 Windows VM 或另一台机器上运行 dist/bin/plc_host.exe
```

**验证清单:**
- 程序启动不报缺少 DLL
- 所有界面显示正常
- SQLite 数据库自动创建
- 日志文件生成

**Commit:** `build: CMake install rules and windeployqt deployment`

---

### Task HARD-07: 最终视觉审查

**Agent:** `@frontend` (Luna)
**Depends on:** HARD-06

Luna 对 Windows 上运行的程序截图做多模态审查:
- 看板编辑器布局和组件是否符合设计规范
- 编辑/运行模式视觉区分是否明显
- 组件状态 (正常/悬停/按下/禁用) 是否正确
- 质量状态颜色是否符合 token
- 不同 DPI 缩放下组件是否清晰
- 列出偏差清单和严重程度

@coder 根据偏差清单修复，修复后 Luna 复查。

---

### Task HARD-08: 最终安全与回归审查

**Agent:** `@reviewer`
**Depends on:** HARD-07

**审查范围 (只读, 不改文件):**
1. 全量 ctest 通过
2. 点动按钮所有释放路径: 松开/失焦/页面切换/模式切换/退出/超时
3. 断线后旧写命令不重放 (grep WriteQueue::clear 和 AcquisitionEngine::cancelPendingWrites 调用点)
4. 编辑模式全路径无 PLC 写入 (grep IModbusClient::sendWriteRequest 调用方, 确认编辑模式分支)
5. 内存泄漏: QGraphicsItem 所有权、QUndoCommand 所有权、信号槽连接生命周期
6. 线程安全: 所有 QObject::moveToThread 调用、跨线程信号参数类型 (必须是值类型或 QMetaType 已注册)
7. 安全: 无硬编码密码、无 SQL 字符串拼接 (全部用 Qt 参数化查询)、无 system/popen/shell 调用
8. 退出路径: MainWindow::closeEvent → AcquisitionEngine::stop → flush 数据库 → quit 线程 → wait

**Commit:** `chore: final safety review checklist and fixes`
