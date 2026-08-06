#pragma once

#include <QJsonObject>
#include <QVariant>

#include "dashboard/DashboardBaseItem.h"
#include "domain/TagValue.h"

// 数值输入组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-06)：编辑模式显示占位输入框样式；运行模式双击弹出 QInputDialog，
// 校验范围后发出 valueSubmitted(tagId, value) 信号（DASH-08+ 将其映射为写命令）。
//
// 业务属性 config["tagId"]、config["min"]（默认 0）、config["max"]（默认 100）、
// config["precision"]（小数位，默认 1）。运行值通过 setTagValue 注入用于显示
// 当前值；双击弹出对话框时以其作为初始值。
class ValueInputItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit ValueInputItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 注入运行值。未设置（value 无效）时显示 "--"。
    void setTagValue(const QVariant& value, Quality quality);
    bool hasValue() const;

    // 当前显示的格式化文本（未取值时为 "--"）。
    QString displayText() const;

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

signals:
    // 运行模式双击输入并通过范围校验后发出。
    void valueSubmitted(int tagId, double value);

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void promptForValue();

    int m_tagId = -1;
    double m_min = 0.0;
    double m_max = 100.0;
    int m_precision = 1;

    // 运行态（不持久化）。
    QVariant m_value;
    Quality m_quality = Quality::Disconnected;
};
