#pragma once

#include <QJsonObject>
#include <QVariant>

#include "dashboard/DashboardBaseItem.h"

// 开关组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-05)：paint 显示 ON/OFF 切换状态；运行模式下点击切换并发出
// toggled 信号（DASH-08 ButtonActionExecutor 将其映射为写命令）。
//
// 业务属性 config["tagId"]、config["onValue"]（ON 对应值）、
// config["offValue"]（OFF 对应值）。编辑模式点击交给基类（选择/拖动），
// 不切换状态；运行模式点击才切换并 emit。运行态 m_on 不持久化。
class SwitchItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit SwitchItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 注入/读取运行态（true = ON）。
    void setOn(bool on);
    bool isOn() const;

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

signals:
    // 运行模式点击切换后发出：携带绑定的 tagId 与切换后的状态。
    void toggled(int tagId, bool on);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    int m_tagId = -1;
    QVariant m_onValue = true;
    QVariant m_offValue = false;

    // 运行态（不持久化）。
    bool m_on = false;
};
