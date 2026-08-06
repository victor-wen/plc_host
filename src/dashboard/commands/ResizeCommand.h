#pragma once

#include <QRectF>
#include <QUndoCommand>

#include "dashboard/DashboardBaseItem.h"

// 撤销栈命令：缩放组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-04)。
//
// rect 为场景坐标下的组件边界（topLeft = 组件 pos，size = boundingRect 尺寸）。
// redo/undo 分别应用 newRect/oldRect；mergeWith 合并同一组件的连续缩放，
// 拖拽缩放手柄整体一次撤销。
class ResizeCommand : public QUndoCommand {
public:
    ResizeCommand(DashboardBaseItem* item, const QRectF& oldRect, const QRectF& newRect,
                  QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;
    int id() const override { return kId; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    static constexpr int kId = 1002; // 命令类型内唯一 ID（合并判定依据）

    void apply(const QRectF& rect);

    DashboardBaseItem* m_item;
    QRectF m_oldRect;
    QRectF m_newRect;
};
