#pragma once

#include <QPointF>
#include <QUndoCommand>

#include "dashboard/DashboardBaseItem.h"

// 撤销栈命令：移动组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-04)。
//
// - redo: setPos(newPos)；undo: setPos(oldPos)。
// - mergeWith: 同一组件的连续移动合并为一次撤销步（拖拽整体一次撤销）。移动
//   增量 ≤ 2px 视为无意义微动（方向键微调/鼠标抖动），不合并，保留独立撤销步。
class MoveCommand : public QUndoCommand {
public:
    MoveCommand(DashboardBaseItem* item, const QPointF& oldPos, const QPointF& newPos,
                QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;
    int id() const override { return kId; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    static constexpr int kId = 1001; // 命令类型内唯一 ID（合并判定依据）

    DashboardBaseItem* m_item;
    QPointF m_oldPos;
    QPointF m_newPos;
};
