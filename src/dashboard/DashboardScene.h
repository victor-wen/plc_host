#pragma once

#include <QGraphicsScene>
#include <QList>
#include <QPointF>

#include "DashboardBaseItem.h"
#include "DashboardDocument.h"

// 看板自由画布 (docs/architecture/interfaces.md §9, docs/qt/qt-widgets-graphics-view.md,
// Phase 2 DASH-02)
//
// - 场景尺寸与背景色来自 DashboardPage（setPage）。
// - addItem(const DashboardItem&) 是组件工厂：按 itemType 创建对应 QGraphicsObject，
//   当前具体组件（DASH-05+）尚未实现，统一创建占位组件；未知/损坏类型按
//   errorPlaceholder 黄色占位。
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
    DashboardBaseItem* addItem(const DashboardItem& meta);

    // 切换全部组件的编辑/运行模式。
    void setEditMode(bool editing);

    // 返回场景中所有 DashboardBaseItem 子类组件（过滤非看板图形项）。
    QList<DashboardBaseItem*> dashboardItems() const;

    // 将坐标吸附到 gridSize 网格（向最近网格点取整）。
    QPointF snapToGrid(QPointF pos, int gridSize = 10) const;

    // 选中项置于最前 / 最后（修改 zValue，保持选中集内部相对顺序）。
    void bringToFront();
    void sendToBack();

    // 按包围盒对齐选中项：AlignLeft/HAlignCenter/AlignRight 与
    // AlignTop/VAlignCenter/AlignBottom。
    void alignSelected(Qt::Alignment alignment);

    // 删除全部选中组件。
    void deleteSelected();

    // 返回选中的 DashboardBaseItem 组件（过滤非看板图形项）。
    QList<DashboardBaseItem*> selectedItems() const;

private:
    // 运行模式标记：新加入的组件应用当前模式。
    bool m_editMode = true;
};
