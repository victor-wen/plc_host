#include "dashboard/commands/AddItemCommand.h"

AddItemCommand::AddItemCommand(DashboardScene* scene, const DashboardItem& meta,
                               QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_scene(scene)
    , m_meta(meta)
{
    setText(QStringLiteral("添加组件"));
}

void AddItemCommand::redo()
{
    if (m_item)
        m_scene->addItem(m_item); // 撤销后重做：复用同一组件，不重复构造
    else
        m_item = m_scene->addItem(m_meta); // 首次执行：经场景工厂创建
}

void AddItemCommand::undo()
{
    if (!m_item)
        return;
    m_item->setSelected(false);
    // 不 delete：scene 持有组件所有权，命令保留指针供 redo 复用。
    m_scene->removeItem(m_item);
}
