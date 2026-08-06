#include "dashboard/commands/ResizeCommand.h"

ResizeCommand::ResizeCommand(DashboardBaseItem* item, const QRectF& oldRect,
                             const QRectF& newRect, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_item(item)
    , m_oldRect(oldRect)
    , m_newRect(newRect)
{
    setText(QStringLiteral("缩放组件"));
}

void ResizeCommand::undo()
{
    apply(m_oldRect);
}

void ResizeCommand::redo()
{
    apply(m_newRect);
}

void ResizeCommand::apply(const QRectF& rect)
{
    m_item->setPos(rect.topLeft());
    m_item->setSize(rect.width(), rect.height());
}

bool ResizeCommand::mergeWith(const QUndoCommand* other)
{
    const auto* resize = dynamic_cast<const ResizeCommand*>(other);
    if (!resize || resize->m_item != m_item)
        return false;
    m_newRect = resize->m_newRect;
    return true;
}
