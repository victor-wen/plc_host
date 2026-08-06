#pragma once

#include <QJsonObject>
#include <QVariant>

#include "dashboard/DashboardBaseItem.h"
#include "domain/TagValue.h"

// 弧形仪表组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-06)：paint 绘制 灰色圆弧背景 + 彩色数值圆弧 + 指针 + 中心值文字。
//
// 业务属性 config["tagId"]、config["min"]（默认 0）、config["max"]（默认 100）、
// config["startAngle"]（默认 225，Qt 角度：0° = 3 点钟方向，逆时针为正）、
// config["spanAngle"]（默认 270，顺时针扫过 270° 留下右下缺口）。
// 表现层属性 commonStyle["arcColor"]（数值圆弧颜色，默认 #34C759）与
// commonStyle["arcWidth"]（圆弧线宽，默认按尺寸自适应）。
// 指针与数值圆弧共用同一角度映射：angle(ratio) = startAngle + spanAngle*ratio，
// 圆上点 = 圆心 + r*(cos, sin)，保证指针指向数值圆弧端点。
class GaugeItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit GaugeItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 注入运行值。未设置（value 无效）时显示 "0"。
    void setTagValue(const QVariant& value, Quality quality);
    bool hasValue() const;

    // 当前值相对 [min,max] 的比例（0..1），越界钳制。
    qreal ratio() const;

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

private:
    int m_tagId = -1;
    double m_min = 0.0;
    double m_max = 100.0;
    qreal m_startAngle = 225.0;
    qreal m_spanAngle = 270.0;

    // 表现层（commonStyle）。
    QColor m_arcColor = QColor(QStringLiteral("#34C759"));

    // 运行态（不持久化）。
    QVariant m_value;
    Quality m_quality = Quality::Disconnected;
};
