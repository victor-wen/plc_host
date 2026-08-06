#include "dashboard/commands/PropertyChangeCommand.h"

PropertyChangeCommand::PropertyChangeCommand(DashboardBaseItem* item, const QString& key,
                                             const QJsonValue& oldValue,
                                             const QJsonValue& newValue,
                                             QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_item(item)
    , m_key(key)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
    , m_inConfig(item->config.contains(key))
{
    setText(QStringLiteral("修改属性"));
}

void PropertyChangeCommand::undo()
{
    apply(m_oldValue);
}

void PropertyChangeCommand::redo()
{
    apply(m_newValue);
}

void PropertyChangeCommand::apply(const QJsonValue& value)
{
    QJsonObject& container = m_inConfig ? m_item->config : m_item->commonStyle;
    if (value.isUndefined())
        container.remove(m_key);
    else
        container.insert(m_key, value);
}
