#include "dashboard/commands/RemoveItemCommand.h"

QJsonObject dashboardItemToJson(const DashboardBaseItem* item)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), item->itemId);
    obj.insert(QStringLiteral("itemType"), item->itemType);
    obj.insert(QStringLiteral("x"), item->pos().x());
    obj.insert(QStringLiteral("y"), item->pos().y());
    obj.insert(QStringLiteral("width"), item->boundingRect().width());
    obj.insert(QStringLiteral("height"), item->boundingRect().height());
    obj.insert(QStringLiteral("zOrder"), item->zValue());
    obj.insert(QStringLiteral("commonStyle"), item->commonStyle);
    obj.insert(QStringLiteral("config"), item->config);
    obj.insert(QStringLiteral("schemaVersion"), item->schemaVersion);
    return obj;
}

DashboardItem dashboardItemFromJson(const QJsonObject& json)
{
    DashboardItem meta;
    meta.id = json.value(QStringLiteral("id")).toInt(-1);
    meta.itemType = json.value(QStringLiteral("itemType")).toString();
    meta.x = json.value(QStringLiteral("x")).toDouble(0.0);
    meta.y = json.value(QStringLiteral("y")).toDouble(0.0);
    meta.width = json.value(QStringLiteral("width")).toDouble(100.0);
    meta.height = json.value(QStringLiteral("height")).toDouble(100.0);
    meta.zOrder = json.value(QStringLiteral("zOrder")).toDouble(0.0);
    meta.commonStyle = json.value(QStringLiteral("commonStyle")).toObject();
    meta.config = json.value(QStringLiteral("config")).toObject();
    meta.schemaVersion = json.value(QStringLiteral("schemaVersion")).toInt(1);
    return meta;
}

RemoveItemCommand::RemoveItemCommand(DashboardScene* scene, DashboardBaseItem* item,
                                     const QJsonObject& snapshot, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_scene(scene)
    , m_item(item)
    , m_snapshot(snapshot)
{
    setText(QStringLiteral("删除组件"));
}

void RemoveItemCommand::redo()
{
    if (!m_item)
        return;
    m_snapshot = dashboardItemToJson(m_item); // 以移除时刻的实时状态为准
    m_item->setSelected(false);
    m_scene->removeItem(m_item);
    delete m_item; // scene 拥有组件所有权
    m_item = nullptr;
}

void RemoveItemCommand::undo()
{
    if (m_item)
        return;
    m_item = m_scene->addItem(dashboardItemFromJson(m_snapshot));
}
