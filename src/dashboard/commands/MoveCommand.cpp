#include "dashboard/commands/MoveCommand.h"

#include <QLineF>

MoveCommand::MoveCommand(DashboardBaseItem* item, const QPointF& oldPos,
                         const QPointF& newPos, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_item(item)
    , m_oldPos(oldPos)
    , m_newPos(newPos)
{
    setText(QStringLiteral("移动组件"));
}

void MoveCommand::undo()
{
    m_item->setPos(m_oldPos);
}

void MoveCommand::redo()
{
    m_item->setPos(m_newPos);
}

bool MoveCommand::mergeWith(const QUndoCommand* other)
{
    const auto* move = dynamic_cast<const MoveCommand*>(other);
    if (!move || move->m_item != m_item)
        return false;
    // 增量 ≤ 2px 视为无意义微动：不合并，保留为独立撤销步。
    if (QLineF(move->m_newPos, m_newPos).length() <= 2.0)
        return false;
    m_newPos = move->m_newPos;
    return true;
}
