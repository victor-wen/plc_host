#pragma once

#include <QGraphicsScene>
#include <QJsonArray>
#include <QList>
#include <QPointF>
#include <QUndoStack>

#include "DashboardBaseItem.h"
#include "DashboardDocument.h"

class QKeyEvent;

// 看板自由画布 (docs/architecture/interfaces.md §9, docs/qt/qt-widgets-graphics-view.md,
// Phase 2 DASH-02)
//
// - 场景尺寸与背景色来自 DashboardPage（setPage）。
// - addItem(const DashboardItem&) 是组件工厂：按 itemType 创建对应 QGraphicsObject
//   （text/rect/image/value/led/switch 为 DASH-05 具体组件，progress/gauge/valueInput/
//   trend 为 DASH-06 高级组件）；未知/损坏类型按 errorPlaceholder 黄色占位。
// - setEditMode(false) 切换运行模式：所有组件不可移动/不可选择。
// - 组件数 <100，使用 NoIndex 线性索引，避免维护 BSP 树的开销。
class DashboardScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit DashboardScene(QObject* parent = nullptr);

    using QGraphicsScene::addItem;

    // 设置画布尺寸与背景色。background 为空或解析失败时使用默认深色背景。
    void setPage(const DashboardPage& page);

    // 工厂方法：按 meta.itemType 创建组件并应用几何与元数据。返回场景拥有的组件。
    // 这是底层工厂（加载页面/命令 redo 使用），不经过撤销栈。
    DashboardBaseItem* addItem(const DashboardItem& meta);

    // 经撤销栈添加组件：push AddItemCommand 并返回其创建的组件。
    // 后续 undo 会将其从场景移除，redo 原样放回。
    DashboardBaseItem* addItemWithUndo(const DashboardItem& meta);

    // 经撤销栈移动/缩放组件（DASH-04）：连续调用会由 MoveCommand/ResizeCommand
    // 的 mergeWith 合并为一次撤销步。
    void moveItem(DashboardBaseItem* item, const QPointF& newPos);
    void resizeItem(DashboardBaseItem* item, const QRectF& newRect);

    // 场景持有的撤销栈（UI 主线程专用，见 docs/architecture/threading.md）。
    QUndoStack* undoStack() { return &m_undoStack; }

    // 切换全部组件的编辑/运行模式。
    void setEditMode(bool editing);

    // 当前模式：true = 编辑，false = 运行（DASH-09 协调器查询用）。
    bool isEditMode() const { return m_editMode; }

    // 返回场景中所有 DashboardBaseItem 子类组件（过滤非看板图形项）。
    QList<DashboardBaseItem*> dashboardItems() const;

    // 顶层项：场景中无父项的图形项（组件本体；DASH-03 缩放手柄等子项不计入）。
    // Qt 6 移除了 QGraphicsScene::topLevelItems()，这里提供等价实现。
    QList<QGraphicsItem*> topLevelItems() const;

    // 将坐标吸附到 gridSize 网格（向最近网格点取整）。
    QPointF snapToGrid(QPointF pos, int gridSize = 10) const;

    // 选中项置于最前 / 最后（修改 zValue，保持选中集内部相对顺序）。
    void bringToFront();
    void sendToBack();

    // DASH-03: 层级微调 ±1（保持选中集内部相对顺序）。
    void stepForward();
    void stepBackward();

    // DASH-03: 复制/粘贴。copySelected 将选中项序列化为 JSON 数组存入场景
    // 剪贴板（同时镜像到系统剪贴板）；pasteClipboard 读回并以原位置 +20px
    // 偏移创建新组件并选中它们。
    void copySelected();
    void pasteClipboard();

    // 按包围盒对齐选中项：AlignLeft/HAlignCenter/AlignRight 与
    // AlignTop/VAlignCenter/AlignBottom。
    void alignSelected(Qt::Alignment alignment);

    // 删除全部选中组件。
    void deleteSelected();

    // 返回选中的 DashboardBaseItem 组件（过滤非看板图形项）。
    QList<DashboardBaseItem*> selectedItems() const;

protected:
    // DASH-03 快捷键（仅编辑模式）：Delete → deleteSelected；
    // Ctrl+C → copySelected；Ctrl+V → pasteClipboard。
    void keyPressEvent(QKeyEvent* event) override;

private:
    // 撤销/重做栈：所有编辑操作（添加/删除/移动/缩放/属性修改）经命令入栈。
    QUndoStack m_undoStack;

    // 运行模式标记：新加入的组件应用当前模式。
    bool m_editMode = true;

    // DASH-03 复制/粘贴中转的组件 JSON 数组（场景剪贴板）。
    QJsonArray m_clipboard;
};
