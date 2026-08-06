#include "recipe/RecipeService.h"

#include <QDebug>
#include <QEventLoop>
#include <QHash>
#include <QMetaObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QVariant>

#include "domain/TagValue.h"
#include "runtime/AcquisitionEngine.h"
#include "runtime/TagCache.h"
#include "runtime/WriteQueue.h"

namespace {

// SQLite datetime('now') 产出 "yyyy-MM-dd HH:mm:ss"（秒级，空格分隔）。
QDateTime fromDbTime(const QString& s)
{
    QDateTime dt = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid())
        dt = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    return dt;
}

} // namespace

RecipeService::RecipeService(const QString& dbConnectionName)
    : m_connectionName(dbConnectionName)
{
}

QVector<Recipe> RecipeService::loadRecipes()
{
    QVector<Recipe> out;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT id, name, description, created_at, updated_at FROM recipes ORDER BY id")))
        return out;

    while (q.next()) {
        Recipe r;
        r.id = q.value(0).toInt();
        r.name = q.value(1).toString();
        r.description = q.value(2).toString();
        r.createdAt = fromDbTime(q.value(3).toString());
        r.updatedAt = fromDbTime(q.value(4).toString());
        out.append(r);
    }
    return out;
}

QVector<RecipeItem> RecipeService::loadRecipeItems(int recipeId)
{
    QVector<RecipeItem> out;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, recipe_id, tag_id, value FROM recipe_items WHERE recipe_id = ? ORDER BY id"));
    q.addBindValue(recipeId);
    if (!q.exec())
        return out;

    while (q.next()) {
        RecipeItem item;
        item.id = q.value(0).toInt();
        item.recipeId = q.value(1).toInt();
        item.tagId = q.value(2).toInt();
        item.value = q.value(3).toDouble();
        out.append(item);
    }
    return out;
}

int RecipeService::createRecipe(const QString& name, const QString& description)
{
    if (name.trimmed().isEmpty())
        return -1;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return -1;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO recipes (name, description) VALUES (?, ?)"));
    q.addBindValue(name.trimmed());
    // null QString 绑定会变成 SQL NULL，触发 NOT NULL 约束，归一化为空字符串
    QString desc = description;
    if (desc.isNull())
        desc = QStringLiteral("");
    q.addBindValue(desc);
    if (!q.exec()) {
        qWarning() << "createRecipe failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool RecipeService::saveItem(int recipeId, int tagId, double value)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;
    if (!db.transaction())
        return false;

    // 配方必须存在
    QSqlQuery chk(db);
    chk.prepare(QStringLiteral("SELECT 1 FROM recipes WHERE id = ?"));
    chk.addBindValue(recipeId);
    if (!chk.exec() || !chk.next()) {
        db.rollback();
        return false;
    }

    // 同一 recipe+tag 幂等：已存在则覆盖，否则插入
    int itemId = -1;
    QSqlQuery sel(db);
    sel.prepare(QStringLiteral("SELECT id FROM recipe_items WHERE recipe_id = ? AND tag_id = ?"));
    sel.addBindValue(recipeId);
    sel.addBindValue(tagId);
    if (sel.exec() && sel.next())
        itemId = sel.value(0).toInt();

    QSqlQuery q(db);
    if (itemId >= 0) {
        q.prepare(QStringLiteral("UPDATE recipe_items SET value = ? WHERE id = ?"));
        q.addBindValue(value);
        q.addBindValue(itemId);
    } else {
        q.prepare(
            QStringLiteral("INSERT INTO recipe_items (recipe_id, tag_id, value) VALUES (?, ?, ?)"));
        q.addBindValue(recipeId);
        q.addBindValue(tagId);
        q.addBindValue(value);
    }
    if (!q.exec()) {
        db.rollback();
        return false;
    }

    QSqlQuery upd(db);
    upd.prepare(QStringLiteral("UPDATE recipes SET updated_at = datetime('now') WHERE id = ?"));
    upd.addBindValue(recipeId);
    if (!upd.exec()) {
        db.rollback();
        return false;
    }
    return db.commit();
}

bool RecipeService::deleteRecipe(int recipeId)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen())
        return false;
    if (!db.transaction())
        return false;

    // 手动删除明细，不依赖 foreign_keys pragma 的开关状态
    QSqlQuery items(db);
    items.prepare(QStringLiteral("DELETE FROM recipe_items WHERE recipe_id = ?"));
    items.addBindValue(recipeId);
    if (!items.exec()) {
        db.rollback();
        return false;
    }

    QSqlQuery recipe(db);
    recipe.prepare(QStringLiteral("DELETE FROM recipes WHERE id = ?"));
    recipe.addBindValue(recipeId);
    if (!recipe.exec()) {
        db.rollback();
        return false;
    }
    return db.commit();
}

QVector<RecipeItem> RecipeService::readFromPlc(TagCache* cache, const QVector<int>& tagIds)
{
    QVector<RecipeItem> out;
    if (cache == nullptr)
        return out;
    out.reserve(tagIds.size());
    for (int tagId : tagIds) {
        RecipeItem item;
        item.tagId = tagId;
        const TagValue tv = cache->value(tagId);
        bool ok = false;
        const double v = tv.value.toDouble(&ok);
        item.value = ok ? v : 0.0;
        out.append(item);
    }
    return out;
}

DownloadResult RecipeService::download(const QVector<RecipeItem>& items,
                                       AcquisitionEngine* engine, int timeoutMs)
{
    DownloadResult result;
    if (engine == nullptr || items.isEmpty())
        return result;

    // tagId → 本批待完成写入数（同一 tag 可能重复出现）
    QHash<int, int> pending;
    for (const RecipeItem& item : items)
        pending[item.tagId]++;

    int completed = 0;
    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);

    const auto onWriteCompleted = [&](int tagId, bool success, const QString& error) {
        auto it = pending.find(tagId);
        if (it == pending.end() || it.value() <= 0)
            return;   // 非本批次的写入回执，忽略
        --it.value();
        if (success) {
            ++result.success;
        } else {
            ++result.failed;
            result.errors.append(QStringLiteral("tag %1: %2").arg(tagId).arg(error));
        }
        if (++completed == items.size())
            loop.quit();
    };

    QObject::connect(engine, &AcquisitionEngine::writeCompleted, &loop, onWriteCompleted);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    watchdog.start(timeoutMs);

    // 逐项入队（AutoConnection：同线程直调，跨线程排队），失败项由引擎回执，不中断后续项
    for (const RecipeItem& item : items) {
        WriteCommand cmd;
        cmd.tagId = item.tagId;
        cmd.value = QVariant(item.value);
        QMetaObject::invokeMethod(engine, "enqueueWrite", Qt::AutoConnection,
                                  Q_ARG(WriteCommand, cmd));
    }

    loop.exec();

    const int remaining = items.size() - completed;
    if (remaining > 0) {
        result.failed += remaining;
        result.errors.append(QStringLiteral("%1 item(s) timed out").arg(remaining));
    }
    return result;
}
