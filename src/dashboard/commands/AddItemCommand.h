#pragma once

#include <QUndoCommand>

#include "dashboard/DashboardDocument.h"
#include "dashboard/DashboardScene.h"

// 撤销栈命令：向场景添加组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-04)。
//
// - redo: 首次经场景工厂 addItem(meta) 创建组件并保存指针；撤销后重做复用同一
//   组件，避免重复构造。
// - undo: 从场景移除组件但不 delete。组件所有权在 scene（位于场景时）与命令
//   （被移除期间）之间转移，redo 可原样放回；命令离开撤销栈且组件仍被移除时
//   由调用方负责清理（评审项 RG-4 跟踪）。
class AddItemCommand : public QUndoCommand {
public:
    AddItemCommand(DashboardScene* scene, const DashboardItem& meta,
                   QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

    // 命令当前持有的组件（redo 后非空；供调用方获取工厂创建的实例）。
    DashboardBaseItem* item() const { return m_item; }

private:
    DashboardScene* m_scene;
    DashboardItem m_meta;
    DashboardBaseItem* m_item = nullptr;
};
