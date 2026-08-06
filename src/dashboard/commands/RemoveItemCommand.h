#pragma once

#include <QJsonObject>
#include <QUndoCommand>

#include "dashboard/DashboardBaseItem.h"
#include "dashboard/DashboardDocument.h"
#include "dashboard/DashboardScene.h"

// 撤销栈命令：从场景移除组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-04)。
//
// - redo: 先以组件实时状态刷新序列化快照，再从场景移除并释放（scene 拥有所有权）。
// - undo: 从快照 JSON 恢复 DashboardItem 元数据，经场景工厂重新创建组件。
// 构造时由调用方传入快照；redo 时再次刷新，防止构造与入栈之间状态漂移。

// 将组件完整状态序列化为 JSON（id/itemType/几何/commonStyle/config/schemaVersion）。
QJsonObject dashboardItemToJson(const DashboardBaseItem* item);

// 从 JSON 恢复组件元数据。
DashboardItem dashboardItemFromJson(const QJsonObject& json);

class RemoveItemCommand : public QUndoCommand {
public:
    RemoveItemCommand(DashboardScene* scene, DashboardBaseItem* item,
                      const QJsonObject& snapshot, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    DashboardScene* m_scene;
    DashboardBaseItem* m_item;
    QJsonObject m_snapshot;
};
