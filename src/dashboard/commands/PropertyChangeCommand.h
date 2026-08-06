#pragma once

#include <QJsonValue>
#include <QUndoCommand>

#include "dashboard/DashboardBaseItem.h"

// 撤销栈命令：修改组件属性 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-04)。
//
// 修改 commonStyle 或 config 中 key 对应的属性。目标容器在构造时确定：key 已在
// config 中则修改 config，否则修改 commonStyle（表现层属性默认归属）。
// 传入 QJsonValue(QJsonValue::Undefined) 表示删除该属性（注意 QJsonValue() 默认
// 构造为 Null，是合法 JSON 值，不代表删除）。
class PropertyChangeCommand : public QUndoCommand {
public:
    PropertyChangeCommand(DashboardBaseItem* item, const QString& key,
                          const QJsonValue& oldValue, const QJsonValue& newValue,
                          QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void apply(const QJsonValue& value);

    DashboardBaseItem* m_item;
    QString m_key;
    QJsonValue m_oldValue;
    QJsonValue m_newValue;
    bool m_inConfig = false; // true → config，false → commonStyle
};
