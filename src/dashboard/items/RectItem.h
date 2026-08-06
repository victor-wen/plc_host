#pragma once

#include <QColor>
#include <QJsonObject>

#include "dashboard/DashboardBaseItem.h"

// 矩形组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-05)：paint 填充矩形。
//
// 表现层属性全部在 commonStyle：
// commonStyle["fillColor"]（默认 #C5CFD8，design-tokens colors.border.default）、
// commonStyle["borderColor"]（默认 #526170，colors.text.secondary）、
// commonStyle["borderWidth"]（默认 1）。
// 无业务 config，serialize 保持基类行为（config 原样返回）。
class RectItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit RectItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

private:
    QColor m_fillColor = QColor(QStringLiteral("#C5CFD8"));
    QColor m_borderColor = QColor(QStringLiteral("#526170"));
    int m_borderWidth = 1;
};
