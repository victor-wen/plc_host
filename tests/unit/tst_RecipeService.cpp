#include <QModbusDataUnit>
#include <QModbusReply>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QVector>

#include "modbus/IModbusClient.h"
#include "recipe/RecipeService.h"
#include "runtime/AcquisitionEngine.h"
#include "runtime/TagCache.h"
#include "storage/DatabaseMigrator.h"

// ---------------------------------------------------------------------------
// FakeModbusClient：实现 IModbusClient，不连接真实网络。
// - 持有 HoldingRegisters 寄存器表；写请求写回，读请求按地址切片返回。
// - 回复经 QTimer::singleShot(0, ...) 异步完成，让引擎先建立 finished 连接。
// ---------------------------------------------------------------------------
class FakeModbusClient : public IModbusClient {
    Q_OBJECT
public:
    explicit FakeModbusClient(int registerCount = 4, QObject* parent = nullptr)
        : IModbusClient(parent)
        , m_registers(registerCount, 0)
    {
    }

    void connectToDevice(const QString&, int, int) override
    {
        m_connected = true;
        emit connected();
    }

    void disconnectFromDevice() override
    {
        if (!m_connected)
            return;
        m_connected = false;
        emit disconnected();
    }

    bool isConnected() const override { return m_connected; }

    QModbusReply* sendReadRequest(const QModbusDataUnit& unit, int serverAddress) override
    {
        if (!m_connected)
            return nullptr;
        QVector<quint16> values;
        values.reserve(int(unit.valueCount()));
        for (int i = 0; i < int(unit.valueCount()); ++i)
            values.append(registerValue(unit.startAddress() + i));
        auto* reply = new QModbusReply(QModbusReply::Common, serverAddress, this);
        QTimer::singleShot(0, reply, [reply, unit, values]() {
            reply->setResult(QModbusDataUnit(unit.registerType(), unit.startAddress(), values));
            reply->setFinished(true);
            emit reply->finished();
        });
        return reply;
    }

    QModbusReply* sendWriteRequest(const QModbusDataUnit& unit, int serverAddress) override
    {
        if (!m_connected)
            return nullptr;
        const int needed = unit.startAddress() + int(unit.valueCount());
        if (m_registers.size() < needed)
            m_registers.resize(needed);
        for (int i = 0; i < int(unit.valueCount()); ++i)
            m_registers[unit.startAddress() + i] = unit.value(i);
        auto* reply = new QModbusReply(QModbusReply::Common, serverAddress, this);
        QTimer::singleShot(0, reply, [reply]() {
            reply->setFinished(true);
            emit reply->finished();
        });
        return reply;
    }

    void setTimeout(int) override {}
    void setNumberOfRetries(int) override {}

    quint16 registerValue(int address) const
    {
        return (address >= 0 && address < m_registers.size()) ? m_registers.at(address) : 0;
    }

private:
    QVector<quint16> m_registers;
    bool m_connected = false;
};

namespace {

Tag makeTag(int id, int address, int intervalMs)
{
    Tag t;
    t.id = id;
    t.name = QStringLiteral("tag%1").arg(id);
    t.registerType = RegisterType::HoldingRegister;
    t.address = address;
    t.dataType = DataType::UInt16;
    t.pollIntervalMs = intervalMs;
    return t;
}

} // namespace

class RecipeServiceTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tempDir;
    int m_connCounter = 0;

    struct DbFixture {
        QString connName;
        QString path;
        QSqlDatabase db;
    };

    // 建立独立连接（每测试唯一名称），WAL + 迁移到最新 schema
    DbFixture openDb(const QString& suffix)
    {
        DbFixture f;
        f.connName = QStringLiteral("recipe_%1_%2").arg(++m_connCounter).arg(suffix);
        f.path = m_tempDir.path() + "/" + f.connName + ".db";
        f.db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), f.connName);
        f.db.setDatabaseName(f.path);
        f.db.open();
        {
            QSqlQuery q(f.db);
            q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        }
        DatabaseMigrator::migrate(f.db);
        return f;
    }

private slots:
    void createRecipe_saveItem_load_roundtrip()
    {
        DbFixture d = openDb("roundtrip");
        QVERIFY(d.db.isOpen());

        RecipeService svc(d.connName);
        const int recipeId = svc.createRecipe(QStringLiteral("配方A"), QStringLiteral("一号线"));
        QVERIFY(recipeId > 0);

        QVERIFY(svc.saveItem(recipeId, 1, 100.0));
        QVERIFY(svc.saveItem(recipeId, 2, 200.5));
        QVERIFY(svc.saveItem(recipeId, 3, -12.75));

        const auto recipes = svc.loadRecipes();
        QCOMPARE(recipes.size(), 1);
        QCOMPARE(recipes[0].id, recipeId);
        QCOMPARE(recipes[0].name, QStringLiteral("配方A"));
        QCOMPARE(recipes[0].description, QStringLiteral("一号线"));

        const auto items = svc.loadRecipeItems(recipeId);
        QCOMPARE(items.size(), 3);
        QCOMPARE(items[0].tagId, 1);
        QCOMPARE(items[0].value, 100.0);
        QCOMPARE(items[1].tagId, 2);
        QCOMPARE(items[1].value, 200.5);
        QCOMPARE(items[2].tagId, 3);
        QCOMPARE(items[2].value, -12.75);
    }

    void saveItem_upserts_and_deleteRecipe_removes_items()
    {
        DbFixture d = openDb("upsert");
        QVERIFY(d.db.isOpen());

        RecipeService svc(d.connName);
        const int recipeId = svc.createRecipe(QStringLiteral("R1"), QString());
        QVERIFY(recipeId > 0);

        // 同一 recipe+tag 重复保存 → 覆盖更新而非新增行
        QVERIFY(svc.saveItem(recipeId, 1, 10.0));
        QVERIFY(svc.saveItem(recipeId, 1, 99.0));
        QCOMPARE(svc.loadRecipeItems(recipeId).size(), 1);
        QCOMPARE(svc.loadRecipeItems(recipeId)[0].value, 99.0);

        // 不存在的配方 → 保存失败
        QVERIFY(!svc.saveItem(9999, 1, 5.0));

        // 空名称 → 创建失败
        QCOMPARE(svc.createRecipe(QStringLiteral("   "), QString()), -1);

        // 删除配方后其明细一并删除
        QVERIFY(svc.deleteRecipe(recipeId));
        QVERIFY(svc.loadRecipes().isEmpty());
        QVERIFY(svc.loadRecipeItems(recipeId).isEmpty());
    }

    void readFromPlc_reads_current_cache_values()
    {
        TagCache cache;
        TagValue v1;
        v1.tagId = 1;
        v1.value = QVariant(42.5);
        v1.quality = Quality::Good;
        TagValue v2;
        v2.tagId = 2;
        v2.value = QVariant(7.0);
        v2.quality = Quality::Good;
        QHash<int, TagValue> in;
        in.insert(1, v1);
        in.insert(2, v2);
        cache.updateValues(in);

        RecipeService svc(QStringLiteral("unused_connection"));
        const QVector<int> tagIds = {1, 2, 3};   // 3 号 tag 无缓存值
        const auto items = svc.readFromPlc(&cache, tagIds);

        QCOMPARE(items.size(), 3);
        QCOMPARE(items[0].tagId, 1);
        QCOMPARE(items[0].value, 42.5);
        QCOMPARE(items[1].tagId, 2);
        QCOMPARE(items[1].value, 7.0);
        QCOMPARE(items[2].tagId, 3);
        QCOMPARE(items[2].value, 0.0);   // 缓存缺失 → 0

        QVERIFY(svc.readFromPlc(nullptr, tagIds).isEmpty());
    }

    void download_writes_items_and_reports_failures()
    {
        DbFixture d = openDb("download");
        QVERIFY(d.db.isOpen());

        RecipeService svc(d.connName);
        const int recipeId = svc.createRecipe(QStringLiteral("batch"), QStringLiteral("下载测试"));
        QVERIFY(recipeId > 0);
        QVERIFY(svc.saveItem(recipeId, 1, 111.0));
        QVERIFY(svc.saveItem(recipeId, 2, 333.0));
        QVERIFY(svc.saveItem(recipeId, 3, 999.0));   // tag 3 只读 → 预期写失败

        FakeModbusClient fake(4);
        TagCache cache;
        AcquisitionEngine engine(&fake, &cache);

        QVector<Tag> tags;
        tags.append(makeTag(1, 0, 20));
        tags.append(makeTag(2, 1, 20));
        Tag t3 = makeTag(3, 2, 20);
        t3.readOnly = true;
        tags.append(t3);
        engine.setTags(tags);
        engine.start(QStringLiteral("127.0.0.1"), 502, 1);
        QTRY_COMPARE(engine.state(), ConnectionState::Online);

        const auto items = svc.loadRecipeItems(recipeId);
        QCOMPARE(items.size(), 3);

        const DownloadResult res = svc.download(items, &engine, 5000);
        QCOMPARE(res.success, 2);
        QCOMPARE(res.failed, 1);
        QVERIFY(!res.errors.isEmpty());

        bool foundTag3Error = false;
        for (const QString& e : res.errors) {
            if (e.contains(QStringLiteral("tag 3")))
                foundTag3Error = true;
        }
        QVERIFY(foundTag3Error);

        // 可写 tag 已下发到 "PLC" 侧；只读 tag 未被写入
        QCOMPARE(fake.registerValue(0), quint16(111));
        QCOMPARE(fake.registerValue(1), quint16(333));
        QCOMPARE(fake.registerValue(2), quint16(0));

        engine.stop();
    }
};

QTEST_MAIN(RecipeServiceTest)
#include "tst_RecipeService.moc"
