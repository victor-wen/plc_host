# Qt SQL：SQLite 线程、WAL 和迁移

> Qt 版本：6.8（官方页面当前显示的补丁版本为 6.8.8）
> 拉取日期：2026-08-06
> 来源：<https://doc.qt.io/qt-6.8/qsqldatabase.html>、<https://doc.qt.io/qt-6.8/qsqlquery.html>、<https://doc.qt.io/qt-6.8/sql-driver.html>、<https://doc.qt.io/qt-6.8/sql-connecting.html>、<https://doc.qt.io/qt-6.8/threads-modules.html>、<https://doc.qt.io/qt-6.8/threads-qobject.html>
> Context7 libraryId：`/websites/doc_qt_io_qt-6`

## QSqlDatabase 的线程模型

Qt 6.8 官方规则是：一个数据库连接只能在创建它的线程中使用；可以用 `QSqlDatabase::moveToThread()` 改变连接和其 driver 的线程亲和性，但必须先确保没有 `QSqlQuery` 绑定在该连接上。项目采用更简单、风险更低的规则：**每个线程独立创建并打开一个命名连接，不跨线程共享或移动连接**。

`QSqlDatabase` 是值类，但复制它只会复制同一个连接的句柄，不会创建独立连接。因此以下写法不能用于线程隔离：

```cpp
// 错误：dbCopy 仍指向同一个底层连接
QSqlDatabase dbCopy = uiThreadDb;
```

正确的线程初始化方式是在目标线程中执行：

```cpp
QSqlDatabase openThreadDatabase(const QString &path,
                                const QString &connectionName)
{
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                         connectionName);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!db.open())
        throw std::runtime_error(db.lastError().text().toStdString());
    return db;
}
```

使用完成后要先让所有 `QSqlQuery` 和 `QSqlDatabase` 句柄离开作用域，再调用 `QSqlDatabase::removeDatabase(connectionName)`；否则 Qt 会警告连接仍被引用。

### 命名连接

连接名称不是数据库文件名。可以有多个连接指向同一个 SQLite 文件：

```cpp
auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                     QStringLiteral("db_thread"));

auto sameThreadDb = QSqlDatabase::database(QStringLiteral("db_thread"));
```

项目约定：

| 线程 | 连接名示例 | 职责 |
|---|---|---|
| 数据库线程 | `db_thread` | 迁移、历史、报警、日志和批量写入 |
| UI 主线程 | 不创建业务数据库连接 | 只接收已查询的值类型快照 |
| 测试线程 | `test_<name>_<threadId>` | 临时数据库和隔离测试 |

如果必须在一个线程中同时读写，仍然使用同一线程的同一个连接；不要把 `QSqlDatabase` 传到工作线程。

## SQLite 驱动和锁行为

`QSQLITE` 是 Qt 自带的 SQLite 3 驱动。SQLite 是进程内、文件型数据库；文件不存在时，默认会尝试创建。Qt 文档提醒：不同事务同时读写相同资源时可能等待，Qt SQLite driver 会在锁定资源上重试，直到达到 `QSQLITE_BUSY_TIMEOUT` 指定的超时。

`QSQLITE_BUSY_TIMEOUT` 的值单位为毫秒，必须在 `open()` 前通过连接选项设置：

```cpp
db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
```

连接选项是分号分隔的名称或 `name=value`；已打开连接设置选项不会生效，需先 close 再重新 open。超时不是无限重试：超时后 `QSqlQuery::exec()` 返回 false，应记录 `lastError()` 并由上层决定重试/降级。

## WAL 模式

Qt 官方 SQL API 没有独立的 `setWalMode()` 函数；WAL 是 SQLite 的 SQL/连接级设置。项目在**每个新连接打开后**执行一次：

```cpp
QSqlQuery pragma(db);
if (!pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"))) {
    qWarning() << "cannot enable WAL:" << pragma.lastError();
}

QSqlQuery synchronous(db);
synchronous.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
```

`PRAGMA journal_mode=WAL` 的返回结果应读取并核对是否为 `wal`；不能假定所有文件系统都允许 WAL。若返回值不是 `wal`，应记录警告并按项目策略拒绝进入写入模式或退回受控的 rollback journal 模式，不要静默声称已启用 WAL。

WAL 允许读者和写者在更多场景下并发，但仍然只有一个写者；它不能替代事务、短查询和 busy timeout。读查询持有活动结果集时会延长锁生命周期；用完 `SELECT` 后及时销毁 `QSqlQuery` 或调用 `finish()`，再提交/回滚写事务。

### 建议的连接初始化顺序

```cpp
QSqlDatabase openSqlite(const QString &path, const QString &name)
{
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!db.open())
        return db;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA journal_mode=WAL")))
        return db;
    if (!q.next() || q.value(0).toString().compare(
            QStringLiteral("wal"), Qt::CaseInsensitive) != 0) {
        qWarning() << "SQLite WAL was not accepted";
    }
    q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    return db;
}
```

Qt 的 `QSqlQuery::exec(QString)` 对 SQLite 一次只接受一条语句，因此 WAL、schema 创建和每个迁移步骤应分别执行，不要把多条语句拼成一个字符串。

## 事务和批量写入

Qt 要求在创建查询前开始事务：

```cpp
if (!db.transaction())
    return false;

QSqlQuery q(db);
q.prepare(QStringLiteral(
    "INSERT INTO history_samples(tag_id, timestamp_ms, value) "
    "VALUES (:tag_id, :timestamp_ms, :value)"));

for (const auto &sample : samples) {
    q.bindValue(QStringLiteral(":tag_id"), sample.tagId);
    q.bindValue(QStringLiteral(":timestamp_ms"), sample.timestampMs);
    q.bindValue(QStringLiteral(":value"), sample.value);
    if (!q.exec()) {
        db.rollback();
        return false;
    }
}

return db.commit();
```

大批量、同结构写入可使用 `QSqlQuery::execBatch()`：

```cpp
if (!db.transaction())
    return false;

QSqlQuery q(db);
q.prepare(QStringLiteral(
    "INSERT INTO history_samples(tag_id, timestamp_ms, value) "
    "VALUES (?, ?, ?)"));
q.addBindValue(tagIds);
q.addBindValue(timestamps);
q.addBindValue(values);
if (!q.execBatch()) {
    db.rollback();
    return false;
}
return db.commit();
```

每个绑定的 `QVariantList` 必须长度相同，元素类型不要在同一批次中混用。事务应尽量短：在事务外准备值类型数据，在事务内只做必要 SQL；发生 `SQLITE_BUSY`/锁超时可退避后重试有限次数，但不能无限阻塞数据库线程。

## 迁移模式

Qt 提供事务、查询和错误报告，但没有项目专用迁移管理器。项目使用 `schema_migrations` 表保存已应用版本：

```sql
CREATE TABLE IF NOT EXISTS schema_migrations (
    version     INTEGER PRIMARY KEY,
    applied_at  TEXT NOT NULL
);
```

每个迁移是一个有序、幂等的函数，启动时在数据库线程执行：

```cpp
struct Migration {
    int version;
    std::function<bool(QSqlDatabase &)> apply;
};

bool migrate(QSqlDatabase &db, const QList<Migration> &migrations)
{
    QSqlQuery create(db);
    if (!create.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS schema_migrations ("
            "version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL)")))
        return false;

    const int current = readCurrentVersion(db);
    for (const auto &migration : migrations) {
        if (migration.version <= current)
            continue;
        if (!db.transaction() || !migration.apply(db)) {
            db.rollback();
            return false;
        }
        QSqlQuery record(db);
        record.prepare(QStringLiteral(
            "INSERT INTO schema_migrations(version, applied_at) "
            "VALUES (:version, :applied_at)"));
        record.bindValue(QStringLiteral(":version"), migration.version);
        record.bindValue(QStringLiteral(":applied_at"),
                        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if (!record.exec() || !db.commit()) {
            db.rollback();
            return false;
        }
    }
    return true;
}
```

实际代码还应检查 `readCurrentVersion()`、每条 DDL、`commit()` 和 `lastError()`。不要在同一个迁移里依赖跨连接的可见性；一个迁移失败必须整笔回滚，应用进入只读恢复路径而不是继续连接 PLC。

## 线程安全约束

- `QSqlDatabase`、`QSqlQuery`、driver 和活动结果集只在其所属线程使用；对象的值拷贝不等于新连接。
- 数据库线程拥有独立事件循环和独立命名连接。UI 线程不得直接调用数据库线程连接的 `exec()`、`transaction()` 或 `lastError()`。
- UI 请求查询/写入时只使用信号/槽或 `QMetaObject::invokeMethod()`，数据库线程完成后用信号返回 `QVariant`/结构体快照。
- QObject 的父对象和线程亲和性必须一致；关闭时先停止新任务，等待数据库线程提交/回滚并关闭连接，再销毁线程。
