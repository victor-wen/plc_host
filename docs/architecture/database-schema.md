# 数据库 Schema 冻结：database-schema.md

> **冻结日期:** 2026-08-06
> **依据:** DOC-03、Phase 1-4 计划、Qt SQL/SQLite WAL 规则（`docs/qt/qt-sqlite-threading.md`）
> **数据库:** SQLite（Qt SQL driver: `QSQLITE`），单文件，WAL 模式
> **状态:** 冻结。V1 表结构与索引由 `DatabaseMigrator` 在数据库线程（连接名 `db_thread`）执行；结构变更必须走 V2+ 迁移，禁止直接改 V1。

## 0. 连接与全局设置（冻结）

每个线程独立连接；唯一常驻连接名为 `db_thread`（数据库线程）。打开连接时设置：

```sql
PRAGMA journal_mode = WAL;          -- 读写并发，UI/备份线程可同时读
PRAGMA synchronous = NORMAL;        -- WAL 下 NORMAL 足够，兼顾性能
PRAGMA foreign_keys = ON;           -- 必须！级联删除依赖此开关（每连接都要设）
PRAGMA busy_timeout = 5000;         -- 对应 QSQLITE_BUSY_TIMEOUT=5000
```

- `foreign_keys` 是**每连接**设置，不在连接作用域持久化；任何打开 `db_thread` 的代码路径都必须先执行。
- 时间统一存 `TEXT`（ISO 8601，`YYYY-MM-DD HH:MM:SS.SSS`，UTC），比较/排序按字典序即时间序。写入侧用 `datetime('now')` / `strftime('%Y-%m-%d %H:%M:%f','now')`。
- 枚举一律存 `INTEGER`（值定义与 C++ 枚举 `enum class ... : int` 一一对应，见 interfaces.md）。

---

## 1. 迁移框架：schema_migrations

```sql
CREATE TABLE schema_migrations (
    version    INTEGER PRIMARY KEY,                        -- 已应用的 schema 版本号（1,2,3...）
    applied_at TEXT    NOT NULL DEFAULT (datetime('now')),
    checksum   TEXT    NOT NULL DEFAULT ''                 -- 迁移脚本哈希，防篡改/防重复执行
);
```

规则（冻结）：

- `DatabaseMigrator::migrate(QSqlDatabase&)` 读取 `MAX(version)`，逐个执行 `V(version+1)` 的 DDL；每次迁移包在**独立事务**中（BEGIN → 全部语句 → INSERT schema_migrations → COMMIT）。
- 迁移失败 → 回滚该事务，`migrate` 返回 false，应用启动中止并提示（数据库保持上一个可用版本）。
- 迁移幂等：`currentVersion()` 与重复执行 `migrate` 不产生副作用。
- **V2+ 迁移方式**（冻结）：
  1. 新文件 `src/storage/migrations/V2_<描述>.sql`（或 C++ 迁移步骤表）。
  2. 规则：**只加不改**——不修改已发布表的结构定义；需要改结构时新建表+数据搬运，或 `ALTER TABLE ADD COLUMN`（SQLite 限制：只能加列，且新列默认值不可变）。
  3. 每次迁移对应一个递增 version 号，迁移文件加入 CMake 资源或嵌入二进制。
  4. 备份恢复前 `verifySchema(filePath)` 校验版本兼容（MON-07）。
- 降级：不支持。恢复旧备份时按版本比较，高于当前支持版本则拒绝恢复。

---

## 2. V1 完整 DDL（冻结）

### 2.1 plc_config —— PLC 连接配置（单行）

```sql
CREATE TABLE plc_config (
    id               INTEGER PRIMARY KEY CHECK (id = 1),   -- 强制单行：只允许 id=1
    name             TEXT    NOT NULL DEFAULT 'PLC',
    host             TEXT    NOT NULL DEFAULT '192.168.1.100',
    port             INTEGER NOT NULL DEFAULT 502,
    unit_id          INTEGER NOT NULL DEFAULT 1,
    timeout_ms       INTEGER NOT NULL DEFAULT 1000,
    retries          INTEGER NOT NULL DEFAULT 2,
    poll_interval_ms INTEGER NOT NULL DEFAULT 500,
    auto_connect     INTEGER NOT NULL DEFAULT 1 CHECK (auto_connect IN (0,1)),
    updated_at       TEXT    NOT NULL DEFAULT (datetime('now'))
);
```

- 对应 C++ `PlcConfig`。写入用 `INSERT OR REPLACE INTO plc_config (id, ...) VALUES (1, ...)` 或先 DELETE 再 INSERT；应用启动时若表空则插入默认行。
- 枚举/开关字段全部带 `CHECK` 约束。

### 2.2 tags —— 变量定义

```sql
CREATE TABLE tags (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    name             TEXT    NOT NULL,
    register_type    INTEGER NOT NULL DEFAULT 3 CHECK (register_type BETWEEN 0 AND 3),  -- RegisterType
    address          INTEGER NOT NULL DEFAULT 0 CHECK (address >= 0),                   -- 零基 PDU 地址
    data_type        INTEGER NOT NULL DEFAULT 2 CHECK (data_type BETWEEN 0 AND 6),      -- DataType
    byte_order       INTEGER NOT NULL DEFAULT 0 CHECK (byte_order BETWEEN 0 AND 3),     -- ByteOrder
    word_order       INTEGER NOT NULL DEFAULT 0 CHECK (word_order IN (0,1)),            -- WordOrder
    bit_position     INTEGER NOT NULL DEFAULT 0,
    bit_length       INTEGER NOT NULL DEFAULT 1,
    scale            REAL    NOT NULL DEFAULT 1.0,
    offset           REAL    NOT NULL DEFAULT 0.0,
    unit             TEXT    NOT NULL DEFAULT '',
    read_only        INTEGER NOT NULL DEFAULT 0 CHECK (read_only IN (0,1)),
    poll_group       INTEGER NOT NULL DEFAULT 0,
    poll_interval_ms INTEGER NOT NULL DEFAULT 500 CHECK (poll_interval_ms IN (200,500,1000,2000,5000)),
    history_enabled  INTEGER NOT NULL DEFAULT 0 CHECK (history_enabled IN (0,1)),
    history_mode     INTEGER NOT NULL DEFAULT 0 CHECK (history_mode IN (0,1)),          -- HistoryMode
    created_at       TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at       TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_tags_register_address ON tags(register_type, address);  -- 轮询组构建按区+地址扫描
CREATE INDEX idx_tags_name ON tags(name);                                -- 名称搜索/去重检查
```

- `address` 存储**零基**；UI 一基显示由界面层转换，入库一律零基。
- 名称唯一性由应用层校验（批量导入时预检），不设 UNIQUE 约束（便于导入修复场景）。

### 2.3 dashboard_pages / dashboard_items —— 看板

```sql
CREATE TABLE dashboard_pages (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT    NOT NULL DEFAULT '新页面',
    width      INTEGER NOT NULL DEFAULT 1920 CHECK (width > 0),
    height     INTEGER NOT NULL DEFAULT 1080 CHECK (height > 0),
    background TEXT    NOT NULL DEFAULT '',               -- #RRGGBB 或图片路径；空=默认
    sort_order INTEGER NOT NULL DEFAULT 0,
    created_at TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE dashboard_items (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    page_id        INTEGER NOT NULL REFERENCES dashboard_pages(id) ON DELETE CASCADE,  -- 删页级联删组件
    item_type      TEXT    NOT NULL,                     -- text/rect/image/value/led/switch/progress/gauge/trend/button/errorPlaceholder
    x              REAL    NOT NULL DEFAULT 0 CHECK (x >= 0),
    y              REAL    NOT NULL DEFAULT 0 CHECK (y >= 0),
    width          REAL    NOT NULL DEFAULT 100 CHECK (width > 0),
    height         REAL    NOT NULL DEFAULT 100 CHECK (height > 0),
    z_order        REAL    NOT NULL DEFAULT 0,
    common_style   TEXT    NOT NULL DEFAULT '{}',        -- JSON（QJsonObject::toJson 序列化）
    config         TEXT    NOT NULL DEFAULT '{}',        -- JSON：tagId、min/max、unit、ButtonAction 等
    schema_version INTEGER NOT NULL DEFAULT 1,
    created_at     TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at     TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_dashboard_items_page ON dashboard_items(page_id);   -- 按页加载
```

- `common_style`/`config` 存 JSON 文本；**schema_version 同时嵌入 config JSON 内部**（`config["schemaVersion"]`）与列中——列值为主，JSON 内值用于损坏检测。
- 损坏 config（JSON 解析失败/类型未知）→ 应用层降级 `errorPlaceholder`，不阻塞页面加载。
- `deletePage` 由 FK `ON DELETE CASCADE` 自动清理 items（依赖 `PRAGMA foreign_keys = ON`）。

### 2.4 alarm_rules / alarm_events —— 报警

```sql
CREATE TABLE alarm_rules (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    tag_id     INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,   -- 删 tag 级联删其规则
    name       TEXT    NOT NULL DEFAULT '',
    alarm_type INTEGER NOT NULL DEFAULT 0 CHECK (alarm_type BETWEEN 0 AND 5),  -- AlarmType: 0=Bool 1=HighHigh 2=High 3=Low 4=LowLow 5=Change
    threshold  REAL    NOT NULL DEFAULT 0,
    delay_ms   INTEGER NOT NULL DEFAULT 0 CHECK (delay_ms >= 0),
    hysteresis REAL    NOT NULL DEFAULT 0 CHECK (hysteresis >= 0),
    severity   INTEGER NOT NULL DEFAULT 1 CHECK (severity BETWEEN 0 AND 2),    -- 0=Info 1=Warning 2=Critical
    enabled    INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0,1)),
    created_at TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_alarm_rules_tag ON alarm_rules(tag_id);

-- 报警事件流水（触发/确认/恢复各一行）；事件是审计数据，删除规则后保留
CREATE TABLE alarm_events (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_id     INTEGER REFERENCES alarm_rules(id) ON DELETE SET NULL,      -- 规则删除保留事件，rule_id 置空
    tag_id      INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,     -- 删 tag 连带其事件（tag 已无意义）
    tag_name    TEXT    NOT NULL DEFAULT '',       -- 快照：事件发生时名称（防改名后历史失真）
    rule_name   TEXT    NOT NULL DEFAULT '',       -- 快照
    severity    INTEGER NOT NULL DEFAULT 1,        -- 快照
    event_type  INTEGER NOT NULL CHECK (event_type IN (0,1,2)),   -- 0=Triggered 1=Acknowledged 2=Recovered
    value       REAL,                              -- 触发/恢复时的值
    message     TEXT    NOT NULL DEFAULT '',
    occurred_at TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_alarm_events_time ON alarm_events(occurred_at);                 -- 历史时间筛选
CREATE INDEX idx_alarm_events_tag_time ON alarm_events(tag_id, occurred_at);     -- 单 tag 时间线/当前报警
```

- "当前报警"查询 = 该 tag 最近事件为 Triggered/Acknowledged（未 Recovered）；由报警引擎在内存维护状态机，事件流只做持久化（MON-04）。
- 快照字段（tag_name/rule_name/severity）保证历史报警列表在规则/名称变更后仍可读。

### 2.5 history_samples —— 历史采样

```sql
CREATE TABLE history_samples (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    tag_id    INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,   -- 删 tag 级联删历史
    timestamp TEXT    NOT NULL,                                         -- ISO 8601 UTC
    value     REAL,                                                     -- 工程值（已 scale/offset）
    quality   INTEGER NOT NULL DEFAULT 0 CHECK (quality BETWEEN 0 AND 3) -- Quality: 0=Good 1=Stale 2=Bad 3=Disconnected
);

CREATE INDEX idx_history_tag_time ON history_samples(tag_id, timestamp);   -- 历史查询（范围扫描）
CREATE INDEX idx_history_time ON history_samples(timestamp);               -- 90 天清理分批 DELETE
```

- 批量写入（HistoryService）：5s 定时或 500 条满 → 单事务批量 INSERT。
- `cleanOldData(retentionDays=90)`：按 `timestamp` 分批 DELETE（每批 1000 行，批间隔 100ms），避免长锁。
- 采样值统一 REAL；Bool 存 0/1，整数 Tag 由应用层做无损换算（float32 尾数 24 位可覆盖 UInt32 精度需求——由应用层保证，schema 不限制）。

### 2.6 recipes / recipe_items —— 配方

```sql
CREATE TABLE recipes (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL,
    description TEXT    NOT NULL DEFAULT '',
    created_at  TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at  TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE recipe_items (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    recipe_id INTEGER NOT NULL REFERENCES recipes(id) ON DELETE CASCADE,   -- 删配方级联删配方项
    tag_id    INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,      -- 删 tag 级联删其配方项
    value     REAL    NOT NULL DEFAULT 0,                                  -- 配方目标值（工程值）
    UNIQUE (recipe_id, tag_id)                                             -- 同一配方内 tag 唯一
);

CREATE INDEX idx_recipe_items_recipe ON recipe_items(recipe_id);
```

- `download`（MON-05）逐项写入 PLC 后不写回数据库（配方值 = 设计值，保持静态）；"从 PLC 创建"时由服务层生成。
- 删 tag 级联删配方项：避免配方引用失效 tag 造成下发混乱（应用层提示用户）。

### 2.7 operation_logs —— 操作日志（数据库侧审计）

```sql
CREATE TABLE operation_logs (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT    NOT NULL DEFAULT (datetime('now')),
    category  TEXT    NOT NULL DEFAULT 'operation' CHECK (category IN ('operation','communication','app')),
    tag_id    INTEGER,                                -- 可空：与 tag 无关的操作（页面/配置）为 NULL
    tag_name  TEXT    NOT NULL DEFAULT '',            -- 快照
    old_value TEXT    NOT NULL DEFAULT '',            -- 旧值（文本，保留类型原样）
    new_value TEXT    NOT NULL DEFAULT '',            -- 新值
    success   INTEGER NOT NULL DEFAULT 1 CHECK (success IN (0,1)),
    error     TEXT    NOT NULL DEFAULT '',
    user      TEXT    NOT NULL DEFAULT ''             -- 本机单用户，预留
);

CREATE INDEX idx_operation_logs_time ON operation_logs(timestamp);
```

- 与文件日志（LogService 的 operation.log 等，MON-06）并行：文件日志面向运维排障，operation_logs 面向审计查询。
- `tag_id` 无 FK：操作日志是审计数据，删除 tag 必须保留日志（tag_name 快照兜底）。

### 2.8 app_settings —— 键值设置

```sql
CREATE TABLE app_settings (
    key        TEXT PRIMARY KEY,
    value      TEXT NOT NULL DEFAULT '',
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);
```

约定键（冻结）：

| key | 值格式 | 用途 |
|---|---|---|
| `draft_<pageId>` | 看板草稿 JSON（页面+组件） | 异常退出草稿恢复（DASH-10） |
| `ui.address_base` | `"0"` / `"1"` | 界面零基/一基输入切换 |
| `ui.language` | `"zh_CN"` / `"en_US"` | 语言（预留） |
| `history.retention_days` | `"90"` | 历史保留天数（可配） |

- 任意模块可加自定义键；`key` 全局唯一，冲突由约定键表管理。

---

## 3. 表关系总览

```
plc_config (单行 id=1)
tags ──┬── dashboard_items.page_id → dashboard_pages (CASCADE)
       ├── alarm_rules.tag_id (CASCADE) ── alarm_events.rule_id (SET NULL)
       ├── alarm_events.tag_id (CASCADE)
       ├── history_samples.tag_id (CASCADE)
       └── recipe_items.tag_id (CASCADE) ── recipes.id (CASCADE)
operation_logs (无 FK，审计快照)
app_settings (无 FK)
```

级联规则汇总：

| 父表 | 子表 | 删除父行时 |
|---|---|---|
| dashboard_pages | dashboard_items | CASCADE 删组件 |
| tags | dashboard_items（config 内引用） | 不自动处理（JSON 内 tagId 由应用层校验为占位） |
| tags | alarm_rules | CASCADE 删规则 |
| alarm_rules | alarm_events | SET NULL（保留事件历史） |
| tags | alarm_events | CASCADE |
| tags | history_samples | CASCADE |
| recipes | recipe_items | CASCADE |
| tags | recipe_items | CASCADE |

---

## 4. 迁移执行位置与验证（冻结）

- 迁移在**数据库线程**启动时执行（连接名 `db_thread` 打开后立即 migrate）。
- 迁移失败 → 应用启动中止并弹窗提示，不进入半初始化状态。
- 测试验证（tst_DatabaseMigrations.cpp，CORE-02）：临时 SQLite 文件 → 迁移 → 全部表存在且结构一致 → `currentVersion` 正确 → 重复迁移幂等。
- 备份/恢复（MON-07）用 `VACUUM INTO` 或 sqlite3 备份 API；恢复前 `verifySchema` 检查版本兼容。

## 5. 变更管理

- V1 为冻结基线。任何表结构/索引/约束变更 → 新增 `V2+` 迁移 + 更新本文件 + @reviewer 复核。
- 索引只加不减（新查询需求 → 新索引 → 评估写入开销）。
- 本文件与 `interfaces.md` 中 C++ 结构体的字段、枚举值必须一一对应；任一方变更需同步。
