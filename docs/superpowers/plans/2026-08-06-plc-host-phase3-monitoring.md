# Phase 3: 工业监控模块

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 实现历史数据、趋势、报警、配方、日志和数据库备份六个监控模块。

**Tech Stack:** C++20, Qt 6.8 Widgets, Qt Charts, Qt SQL (SQLite WAL), CMake

## Global Constraints

- 历史数据保留 90 天，分批清理避免长锁。
- 报警: 布尔触发、高高/高/低/低低、变化报警，延时和回差可配。
- 配方: 读 PLC 生成、编辑、批量下发前差异确认。
- 日志按大小轮转保留 30 天。
- CSV 导出含 UTF-8 BOM。
- 每项任务 TDD: 失败测试 → 确认 → 实现 → 通过 → 全量 ctest → @reviewer。
- MON-01/MON-04/MON-05 各自完成后立即独立审查，不积累到最后。

---

### Task MON-01: 历史数据服务

**Agent:** `@coder`
**Depends on:** CORE-02, CORE-06

**Files to create:**
- `src/history/HistoryService.h/cpp`
- `tests/unit/tst_HistoryService.cpp`

**HistoryService (数据库线程 QObject):**
- `enqueueSample(TagValue)` → 放入内存队列
- 定时 (5s) 或队列满 (500 条) 批量 INSERT
- 批量写入用事务: BEGIN → INSERT → COMMIT
- `query(int tagId, QDateTime from, QDateTime to) -> QList<TagValue>`
- `cleanOldData(int retentionDays=90)` → 分批 DELETE (每次 1000 行, 间隔 100ms)
- 被采集引擎的数据库线程持有

**tst_HistoryService.cpp 测试:**
- 临时 SQLite 验证插入和查询
- 批量加事务后行数正确
- cleanOldData 只删除 90 天前数据
- 分批清理不产生长时间锁

**Commit:** `feat: history service with batch insert, period query, and 90-day retention`

---

### Task MON-02: CSV 导出

**Agent:** `@coder`
**Depends on:** MON-01, CORE-09

**Files to create:**
- `src/history/CsvExporter.h/cpp`
- `tests/unit/tst_CsvExporter.cpp`

**CsvExporter:**
- `exportHistory(const QString& filePath, const QList<TagValue>& data, const QList<Tag>& tags) -> bool`
- 列: 时间, Tag名称, 值, 质量, 单位
- UTF-8 BOM 头 (EF BB BF)
- 数值保留合理小数位
- `exportTags(const QString& filePath, const QList<Tag>& tags) -> bool`
- 列: 名称, 寄存器类型, 地址, 数据类型, 字节序, 倍率, 偏移, 单位

**tst_CsvExporter.cpp 测试:**
- 导出后文件存在且 BOM 正确
- 内容行数和列匹配
- 浮点数格式正确
- 空列表导出文件仅含表头

**Commit:** `feat: CSV exporter for history data and tag definitions with UTF-8 BOM`

---

### Task MON-03: 趋势服务与趋势页面

**Agent:** `@coder`
**Depends on:** MON-01

**Files to create:**
- `src/history/TrendService.h/cpp`
- `src/ui/TrendWidget.h/cpp`
- `tests/ui/tst_TrendWidget.cpp`

**TrendService (UI 线程):**
- `subscribe(int tagId, int historySeconds=300)`
- `onTagValueUpdated(TagValue)` → 追加到 QLineSeries
- 限制 QLineSeries 点数 (historySeconds * 2 点/秒)
- 历史查询异步，数据返回后填充 series

**TrendWidget (QWidget):**
- 内嵌 QChartView + QChart
- 左侧: 已订阅 tag 列表 (checkbox 切换显示)
- 右侧: QChart 显示多条 QLineSeries
- 底部: 时间范围选择 (1m/5m/15m/1h/6h/24h/自定义)
- 鼠标缩放和游标读数
- "导出数据" 按钮调用 CsvExporter

**tst_TrendWidget.cpp 测试:**
- 订阅 tag 后 series 出现
- TagValue 更新追加到 series
- 点数超限时旧点被移除
- 取消订阅后 series 消失

**Commit:** `feat: trend service with real-time chart, subscription, and history query`

---

### Task MON-04: 报警引擎

**Agent:** `@coder`
**Depends on:** CORE-06

**Files to create:**
- `src/alarm/AlarmEngine.h/cpp`
- `src/alarm/AlarmRule.h`
- `src/ui/AlarmWidget.h/cpp`
- `tests/unit/tst_AlarmEngine.cpp`

**AlarmRule:**
```cpp
enum class AlarmType { Bool = 0, HighHigh = 1, High = 2, Low = 3, LowLow = 4, Change = 5 };
enum class Severity { Info = 0, Warning = 1, Critical = 2 };
struct AlarmRule { int id; int tagId; QString name; AlarmType type; double threshold; int delayMs=0; double hysteresis=0; Severity severity=Severity::Warning; bool enabled=true; };
```

**AlarmEngine (通信线程 QObject):**
- `evaluate(TagValue)` → 检查所有启用的 alarm_rules
- 状态机: Normal → (条件满足 + delayMs) → Triggered → (操作员确认) → Acknowledged → (条件消除) → Recovered
- Hysteresis: 高报警恢复需低于 threshold-hysteresis, 低报警恢复需高于 threshold+hysteresis
- 记录 alarm_events 到数据库线程

**AlarmWidget (QWidget):**
- 两标签: "当前报警" (未恢复的) 和 "历史报警"
- 当前报警: 红色/橙色行，右键确认
- 历史报警: 全部 alarm_events，时间筛选
- 顶部状态栏报警计数徽章

**tst_AlarmEngine.cpp 测试:**
- Bool 型: tag 值变 1 → 延时后触发 → 确认 → 值变 0 → 恢复
- HighHigh, High, Low, LowLow 阈值和回差
- 延时内条件消失则不触发
- 禁用的规则不触发

**Commit:** `feat: alarm engine with 6 alarm types, delay, hysteresis, ack/recover state machine`

---

### Task MON-05: 配方服务

**Agent:** `@coder`
**Depends on:** CORE-06, CORE-07

**Files to create:**
- `src/recipe/RecipeService.h/cpp`
- `src/ui/RecipeWidget.h/cpp`
- `tests/unit/tst_RecipeService.cpp`

**RecipeService:**
- `createFromPLC(QString name, QString desc, QList<int> tagIds)` → 从 TagCache 读取当前值生成配方
- `saveRecipe(Recipe recipe, QList<RecipeItem> items)`
- `loadRecipes() -> QList<Recipe>`
- `loadRecipeItems(int recipeId) -> QList<RecipeItem>`
- `download(QList<RecipeItem> items)` → 逐项写入 PLC
  - 每项写入后等待应答
  - 写入失败记录并继续下一项
  - 返回结果: 成功数和失败列表

**RecipeWidget:**
- 左侧配方列表 (新建/复制/编辑/删除)
- 右侧配方项表格 (Tag名, 当前PLC值, 配方值, 差异高亮)
- "读取PLC值" 按钮 → 填充"当前PLC值"列
- "下发到PLC" 按钮 → 显示差异确认对话框 → 逐项写入
- "新建配方从PLC" → 从当前 PLC 值创建

**tst_RecipeService.cpp 测试:**
- createFromPLC 正确捕获 TagCache 当前值
- download 逐项写入, 中间某项失败后继续后续项
- download 返回成功数和失败明细
- 空配方下载返回成功

**Commit:** `feat: recipe service with PLC read-back, batch download, and difference confirmation`

---

### Task MON-06: 日志服务

**Agent:** `@coder`
**Depends on:** CORE-02

**Files to create:**
- `src/logging/LogService.h/cpp`
- `tests/unit/tst_LogService.cpp`

**LogService:**
- 三类日志文件: operation.log, communication.log, app.log
- 按大小轮转 (默认 5MB), 保留 30 天
- `logOperation(int tagId, double oldVal, double newVal, bool success, QString error)`
- `logCommunication(QString level, QString message)`
- `logApp(QString level, QString message)`
- 日志格式: `[2026-08-06 14:30:00.123] [LEVEL] message`
- 使用 QTextStream 追加写, QMutex 保护

**tst_LogService.cpp 测试:**
- 写入后文件存在
- 超过 5MB 自动轮转 (新文件命名: operation.1.log)
- 30 天前日志文件删除
- 并发多线程写入不崩溃
- 日志格式正确

**Commit:** `feat: log service with size rotation, 30-day retention, and thread safety`

---

### Task MON-07: 数据库备份与恢复

**Agent:** `@coder`
**Depends on:** CORE-02, MON-01

**Files to create:**
- `src/storage/BackupService.h/cpp`
- `src/ui/BackupRestoreDialog.h/cpp`
- `tests/unit/tst_BackupService.cpp`

**BackupService:**
- `backup(QString filePath) -> bool`:
  - 执行 `sqlite3_backup_init` 或 `VACUUM INTO 'path'`
  - 备份后验证文件存在且大小 >0
- `restore(QString filePath) -> bool`:
  - 先备份当前数据库到 timestamp 文件
  - 验证备份文件 schema 版本兼容
  - 替换数据库文件
  - 失败时恢复原数据库
- `verifySchema(QString filePath) -> bool`: 检查 schema_migrations 版本

**BackupRestoreDialog:**
- "备份配置" 按钮 → QFileDialog::getSaveFileName → backup
- "恢复配置" 按钮 → QFileDialog::getOpenFileName → 确认 → restore
- 备份/恢复结果提示

**tst_BackupService.cpp 测试:**
- backup 产生有效文件
- verifySchema 版本兼容/不兼容
- restore 替换数据库后数据一致
- restore 失败时原数据库不变

**Commit:** `feat: database backup/restore with version verification and rollback`

---

## Phase 3 审查节点

| 节点 | 触发 | 审查重点 |
|---|---|---|
| RG-8 | MON-01 完成 | 批量写入事务、清理分批逻辑、SQL 注入防护 |
| RG-9 | MON-04 完成 | 状态机正确性、延时/回差边界、并发 alarm_events 写入 |
| RG-10 | MON-05 完成 | 逐项写入失败继续、差异确认、批量下发的 PLC 安全性 |
| RG-11 | MON-07 完成 | @reviewer 全量监控模块集成测试 |
