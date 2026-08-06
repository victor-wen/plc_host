#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

class TagCache;
class AcquisitionEngine;

// 配方（MON-05）。对应 recipes 表。
struct Recipe {
    int id = -1;
    QString name;
    QString description;
    QDateTime createdAt;
    QDateTime updatedAt;
};

// 配方明细（MON-05）。对应 recipe_items 表。
struct RecipeItem {
    int id = -1;
    int recipeId = -1;
    int tagId = -1;
    double value = 0.0;
};

// 配方下发结果（MON-05）。
struct DownloadResult {
    int success = 0;      // 成功写入项数
    int failed = 0;       // 失败项数（含超时）
    QStringList errors;   // 逐条错误信息
};

// 配方服务：通过 dbConnectionName 使用调用线程预先建立好的 SQLite 连接
// （SQLite 每线程独立连接，WAL 模式）。
// 负责配方的增删查、从采集缓存读取当前值、以及逐项下发到 PLC。
class RecipeService {
public:
    explicit RecipeService(const QString& dbConnectionName);

    // 配方列表，按 id 升序
    QVector<Recipe> loadRecipes();

    // 某配方下的明细，按 id 升序
    QVector<RecipeItem> loadRecipeItems(int recipeId);

    // 新建配方，返回新 id；名称为空或失败返回 -1
    int createRecipe(const QString& name, const QString& description);

    // 保存明细（同一 recipe+tag 覆盖更新）；配方不存在返回 false
    bool saveItem(int recipeId, int tagId, double value);

    // 删除配方及其全部明细
    bool deleteRecipe(int recipeId);

    // 从缓存读取 tagIds 的当前工程值（缓存缺失按 0 处理）
    QVector<RecipeItem> readFromPlc(TagCache* cache, const QVector<int>& tagIds);

    // 逐项下发，失败继续；timeoutMs 内未收到完成通知的项按失败计。
    // 引擎写入采用异步 enqueueWrite + writeCompleted 回执聚合。
    DownloadResult download(const QVector<RecipeItem>& items, AcquisitionEngine* engine,
                            int timeoutMs = 10000);

private:
    QString m_connectionName;
};
