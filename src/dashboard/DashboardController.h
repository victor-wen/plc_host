#pragma once

#include <QObject>
#include <QVector>

#include "DashboardDocument.h"

class DashboardScene;
class DashboardView;
class DashboardRepository;
class ButtonActionExecutor;

// 看板顶层协调器 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md, Task DASH-09)
//
// 职责：
// - 页面生命周期：loadPage / switchPage / createPage / deletePage，数据经
//   DashboardRepository 持久化。
// - 编辑/运行模式隔离：setEditMode 同步切换 DashboardScene 与 DashboardView，
//   保证编辑模式可移动/可选、运行模式锁定布局。
// - 点动安全：页面切换与模式切换前先 releaseAllMomentary（全局约束：点动按钮
//   最大保持 3s，退出运行模式/切页时释放，见计划 RG-5/RG-6）。
// - 页面跳转：连接 ButtonActionExecutor::pageNavigationRequested → switchPage，
//   使运行模式按钮的 NavigatePage 动作可完成跨页导航。
class DashboardController : public QObject {
    Q_OBJECT
public:
    explicit DashboardController(DashboardScene* scene, DashboardView* view,
                                 DashboardRepository* repository,
                                 ButtonActionExecutor* executor,
                                 QObject* parent = nullptr);

    // 当前加载的页面 id；-1 = 尚未加载页面。
    int currentPageId() const { return m_currentPageId; }

    // 当前模式：true = 编辑，false = 运行。
    bool isEditMode() const { return m_editMode; }

    // 全部页面（来自仓储，按 sort_order 排序）。
    QVector<DashboardPage> pages() const;

    // 加载页面到场景：清空场景与撤销栈 → 设置画布 → 从仓储加载 items → addItem
    // → 应用当前模式。
    void loadPage(int pageId);

    // 新建页面并持久化；成功返回新页面 id，失败返回 -1。
    int createPage(const QString& name);

    // 切换到指定页面：先释放全部点动，再加载页面。
    void switchPage(int pageId);

    // 切换编辑/运行模式：同步场景与视图的模式，先释放全部点动。
    void setEditMode(bool editing);

    // 删除页面（仓储级联删除 items）。当前页被删除时清空场景。
    void deletePage(int pageId);

    // 持有的按钮动作执行器（运行模式动作执行入口）。
    ButtonActionExecutor* actionExecutor() const { return m_executor; }

private:
    DashboardScene* m_scene = nullptr;
    DashboardView* m_view = nullptr;
    DashboardRepository* m_repository = nullptr;
    ButtonActionExecutor* m_executor = nullptr;

    int m_currentPageId = -1;
    bool m_editMode = true; // 初始为编辑模式
};
