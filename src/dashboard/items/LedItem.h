#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>

#include "dashboard/DashboardBaseItem.h"

// 指示灯组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-05)：paint 画圆形灯，直径 = min(width, height)，可附加标签文字。
//
// 业务属性 config["tagId"]、config["onColor"]（亮色，默认 #34C759）、
// config["offColor"]（灭色，默认 #8E8E93）、config["label"]（标签，默认空）。
// 运行态通过 setOn 注入（TagCache 集成在 DASH-09+）。
class LedItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit LedItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 注入运行态：true = 亮（onColor），false = 灭（offColor）。
    void setOn(bool on);
    bool isOn() const;

    // 标签文字（config["label"] 的便捷访问；setLabel 同步写入 config）。
    QString label() const;
    void setLabel(const QString& label);

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

private:
    int m_tagId = -1;
    QColor m_onColor = QColor(QStringLiteral("#34C759"));
    QColor m_offColor = QColor(QStringLiteral("#8E8E93"));
    QString m_label;

    // 运行态（不持久化）。
    bool m_on = false;
};
