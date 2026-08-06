# PLC 上位机实施计划总览

**项目:** plc_host
**仓库:** /home/victor/c_proj/plc_host
**目标:** Windows 单 PLC Modbus TCP 上位机，自由画布可编辑看板 + 可配置按钮

## 文档索引

| 文件 | 内容 |
|---|---|
| [spec](../specs/2026-08-06-plc-host-dashboard-design.md) | 完整设计规范 |
| [phase0](2026-08-06-plc-host-phase0-docs.md) | Qt 文档拉取 + 架构冻结 |
| [phase1](2026-08-06-plc-host-phase1-core.md) | CMake/领域模型/Modbus 通信/采集引擎/写队列/变量表 UI |
| [phase2](2026-08-06-plc-host-phase2-dashboard.md) | 自由画布看板/全部组件/按钮动作/编辑运行双模式/撤销 |
| [phase3-5](2026-08-06-plc-host-phase3-5-luna-design.md) | Luna 视觉设计 (与 Phase1 并行) |
| [phase3](2026-08-06-plc-host-phase3-monitoring.md) | 历史/趋势/报警/配方/日志/备份 |
| [phase4](2026-08-06-plc-host-phase4-hardening.md) | 压力测试/故障注入/实机验收/Windows 发布 |

## 执行顺序

```
Phase 0 (DOC-01→DOC-02→DOC-03 架构冻结)
    │
    ├── Phase 1 (CORE-01 ... CORE-09) 核心通信
    │       │
    │       ├── Phase 2 (DASH-01 ... DASH-11) 看板
    │       │
    │       └── Phase 3 (MON-01 ... MON-07) 监控
    │             │
    │             └── Phase 4 (HARD-01 ... HARD-08) 稳定性发布
    │
    └── Phase 3.5 (UI-01 ... UI-07) Luna 视觉设计 (与 Phase1 并行)
```

## Agent 分工

| Agent | 模型 | 职责 |
|---|---|---|
| `@frontend` | opencode-go/gpt-5.6-luna | Qt 文档拉取、视觉设计、多模态截图审查 |
| `@coder` | deepseek-v4-flash | 所有 C++/Qt 编码和单元测试 |
| `@architect` | deepseek-v4-flash | 接口冻结、架构审查 |
| `@reviewer` | deepseek-v4-flash | 代码审查、安全检查、测试覆盖审查 |
| `@debugger` | deepseek-v4-flash | 复杂故障定位 (只读) |
| `@docs-scout` | deepseek-v4-flash | 文档准确性审查 |

## 每项编码任务的固定门禁

1. `@coder` 编写失败的单元测试
2. 运行目标测试，确认因缺少行为而失败
3. `@coder` 实现最小功能
4. 运行目标测试，确认通过
5. 运行全量 `ctest --test-dir build --output-on-failure`
6. `@reviewer` 对 diff 做需求符合性审查
7. `@coder` 修复审查问题
8. `@reviewer` 复审
9. 再次运行全量测试
10. 通过后开始依赖此接口的下一任务

## 构建命令

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$env:QT_ROOT -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Debugger 介入规则

- 同一测试连续修复两次仍失败
- 偶发失败或只在全量测试中失败
- Qt 跨线程对象警告
- 数据库锁/死锁/关闭崩溃
- Modbus 回复与请求错配
- 点动按钮释放动作偶发丢失
- 开发机与部署机行为不同
