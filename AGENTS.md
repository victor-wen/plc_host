# plc_host 项目约定

## 技术栈
- C++20, Qt 6.8 LTS, CMake 3.24+, MSVC 2022 x64, Ninja
- Qt SerialBus, Qt SQL (SQLite WAL), Qt Charts
- Qt Test + CTest

## 组织结构
- `src/app/` - 应用入口和主窗口
- `src/domain/` - 领域模型 (PlcConfig, Tag, TagValue)
- `src/modbus/` - Modbus 通信 (IModbusClient, QtModbusClient, ValueCodec, PollPlanner)
- `src/runtime/` - 运行时 (AcquisitionEngine, TagCache, WriteQueue)
- `src/dashboard/` - 看板 (Scene, View, Repository, Items, Commands, Runtime)
- `src/history/` - 历史数据服务
- `src/alarm/` - 报警引擎
- `src/recipe/` - 配方管理
- `src/storage/` - 数据库迁移和备份
- `src/logging/` - 日志服务
- `src/ui/` - 通用 UI 组件
- `tests/unit/` - 单元测试
- `tests/integration/` - 集成测试
- `tests/ui/` - UI 测试
- `tests/stress/` - 压力测试
- `tests/mock/` - 测试用 Mock 服务器
- `docs/` - 设计文档和实施计划

## 全局约束
- 单台 PLC, 最多 1000 Tag, 默认 500ms 采集
- 编辑模式严禁 PLC 写请求
- QObject 跨线程仅用信号/槽或 QMetaObject::invokeMethod
- SQLite 每线程独立连接, WAL 模式
- 每项编码任务 TDD: 失败测试 → 实现 → 通过 → 全量 ctest → review

## 构建与验证
```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$env:QT_ROOT -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Subagent 说明
本机定义了以下 subagent:
- `@frontend` (gpt-5.6-luna): Qt 文档拉取、视觉设计、多模态审查
- `@coder` (deepseek-v4-flash): 所有 C++ 编码和测试
- `@reviewer` (deepseek-v4-flash): 只读代码审查
- `@architect` (deepseek-v4-flash): 架构设计和接口冻结
- `@debugger` (deepseek-v4-flash): 复杂故障定位
- `@docs-scout` (deepseek-v4-flash): 文档准确性审查
- `@explorer` (deepseek-v4-flash): 代码库探索
