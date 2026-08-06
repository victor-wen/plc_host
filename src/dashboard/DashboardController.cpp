#include "DashboardController.h"

#include "dashboard/runtime/ButtonActionExecutor.h"
#include "dashboard/DashboardRepository.h"
#include "dashboard/DashboardScene.h"
#include "dashboard/DashboardView.h"
#include "dashboard/DashboardBaseItem.h"

#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

DashboardController::DashboardController(DashboardScene* scene, DashboardView* view,
                                         DashboardRepository* repository,
                                         ButtonActionExecutor* executor, QObject* parent)
    : QObject(parent)
    , m_scene(scene)
    , m_view(view)
    , m_repository(repository)
    , m_executor(executor)
{
    // 建立模式不变量：场景/视图与控制器当前模式一致（视图默认运行态 NoDrag，
    // 显式同步到控制器初始编辑态）。
    if (m_scene)
        m_scene->setEditMode(m_editMode);
    if (m_view)
        m_view->setEditMode(m_editMode);

    // 运行模式按钮的 NavigatePage 动作：执行器发出 pageNavigationRequested，
    // 控制器连接后完成跨页跳转（DASH-09 页面跳转职责）。
    if (m_executor) {
        connect(m_executor, &ButtonActionExecutor::pageNavigationRequested, this,
                [this](int pageId) { switchPage(pageId); });
    }
}

QVector<DashboardPage> DashboardController::pages() const
{
    return m_repository->loadPages();
}

void DashboardController::loadPage(int pageId)
{
    // 清空场景与撤销栈：QGraphicsScene::clear() 删除全部组件；撤销栈中持裸
    // item 指针的命令（Move/Resize/Remove）必须一并清空，否则悬垂访问。
    m_scene->clear();
    m_scene->undoStack()->clear();
    m_currentPageId = pageId;

    // 查找页面元数据以设置画布尺寸/背景；页已被删除时用默认画布（DASH-01 默认）。
    DashboardPage page;
    const auto all = m_repository->loadPages();
    for (const auto& p : all) {
        if (p.id == pageId) {
            page = p;
            break;
        }
    }
    m_scene->setPage(page);

    // 从仓储加载组件并加入场景（工厂 addItem 应用场景当前模式）。
    const auto items = m_repository->loadItems(pageId);
    for (const auto& meta : items)
        m_scene->addItem(meta);

    // 新加载组件应用当前模式（运行模式下布局锁定）。
    m_scene->setEditMode(m_editMode);
}

int DashboardController::createPage(const QString& name)
{
    DashboardPage page;
    page.name = name;
    if (!m_repository->savePage(page))
        return -1;
    return page.id;
}

void DashboardController::switchPage(int pageId)
{
    if (pageId == m_currentPageId)
        return; // 同一页：不重复释放、不重复加载。
    // 页面切换前释放全部点动按钮（全局约束：点动保持不超过 3s，切页时释放，
    // 见计划 RG-5/RG-6）。
    m_executor->releaseAllMomentary();
    loadPage(pageId);
}

void DashboardController::setEditMode(bool editing)
{
    if (m_editMode == editing)
        return;
    m_editMode = editing;
    // 离开运行模式/模式切换前释放全部点动按钮（全局约束，计划 RG-5/RG-6）。
    m_executor->releaseAllMomentary();
    m_scene->setEditMode(editing);
    if (m_view)
        m_view->setEditMode(editing);
}

void DashboardController::deletePage(int pageId)
{
    m_repository->deletePage(pageId);
    // 当前页被删除：清空场景与撤销栈，回到未加载状态。
    if (pageId == m_currentPageId) {
        m_scene->clear();
        m_scene->undoStack()->clear();
        m_currentPageId = -1;
        m_dirty = false;
    }
}

void DashboardController::setDirty(bool dirty)
{
    m_dirty = dirty;
}

bool DashboardController::save()
{
    if (m_currentPageId < 0)
        return false;

    auto items = m_scene->dashboardItems();
    QVector<DashboardItem> metaList;
    for (auto* item : items) {
        if (!item)
            continue;

        DashboardItem meta;
        meta.id = item->itemId;
        meta.pageId = m_currentPageId;
        meta.itemType = item->itemType;
        meta.x = item->pos().x();
        meta.y = item->pos().y();
        meta.width = item->boundingRect().width();
        meta.height = item->boundingRect().height();
        meta.zOrder = item->zValue();
        meta.commonStyle = item->commonStyle;
        meta.config = item->config;
        meta.schemaVersion = item->schemaVersion;

        if (meta.itemType.isEmpty())
            return false;
        if (meta.width < 1 || meta.height < 1)
            return false;

        metaList.append(meta);
    }

    if (!m_repository->saveItems(m_currentPageId, metaList))
        return false;

    m_dirty = false;
    return true;
}

void DashboardController::saveDraft()
{
    if (m_currentPageId < 0)
        return;

    QJsonArray arr;
    auto items = m_scene->dashboardItems();
    for (auto* item : items) {
        if (!item)
            continue;
        QJsonObject obj;
        obj["itemType"] = item->itemType;
        obj["x"] = item->pos().x();
        obj["y"] = item->pos().y();
        obj["w"] = item->boundingRect().width();
        obj["h"] = item->boundingRect().height();
        obj["z"] = item->zValue();
        obj["style"] = item->commonStyle;
        obj["config"] = item->config;
        arr.append(obj);
    }

    QSettings settings;
    settings.setValue(QString("dashboard/draft_%1").arg(m_currentPageId),
                      QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

bool DashboardController::restoreDraft()
{
    if (m_currentPageId < 0)
        return false;

    QSettings settings;
    QByteArray data = settings.value(QString("dashboard/draft_%1").arg(m_currentPageId)).toByteArray();
    if (data.isEmpty())
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
        return false;

    m_scene->clear();
    m_scene->undoStack()->clear();

    for (const auto& val : doc.array()) {
        QJsonObject obj = val.toObject();
        DashboardItem meta;
        meta.itemType = obj["itemType"].toString();
        meta.x = obj["x"].toDouble();
        meta.y = obj["y"].toDouble();
        meta.width = obj["w"].toDouble();
        meta.height = obj["h"].toDouble();
        meta.zOrder = obj["z"].toDouble();
        meta.commonStyle = obj["style"].toObject();
        meta.config = obj["config"].toObject();
        meta.pageId = m_currentPageId;
        m_scene->addItem(meta);
    }

    m_scene->setEditMode(m_editMode);
    m_dirty = true;
    return true;
}
