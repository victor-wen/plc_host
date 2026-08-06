#pragma once

#include <QColor>
#include <QJsonObject>
#include <QVariant>

#include "dashboard/DashboardBaseItem.h"
#include "domain/TagValue.h"

// 水平进度条组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-06)：paint 绘制 背景矩形 + 按值填充的进度矩形。
//
// 业务属性 config["tagId"]、config["min"]（默认 0）、config["max"]（默认 100）。
// 填充比例 ratio = clamp((value-min)/(max-min), 0, 1)。表现层属性在 commonStyle：
// commonStyle["barColor"]（填充色，默认 #34C759）、commonStyle["backgroundColor"]
// （背景色，默认 #2A2E38）、commonStyle["showValue"]（是否在条中央显示百分比，
// 默认 true）。运行值通过 setTagValue 注入（TagCache 集成在 DASH-09+），
// 未取值时填充为 0 且文本显示 "--"。
class ProgressBarItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit ProgressBarItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 注入运行值。未设置（value 无效）时填充为 0。
    void setTagValue(const QVariant& value, Quality quality);
    bool hasValue() const;

    // 当前填充比例（0..1），越界值钳制到 [0,1]。
    qreal ratio() const;

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

private:
    int m_tagId = -1;
    double m_min = 0.0;
    double m_max = 100.0;

    // 表现层（commonStyle）。
    QColor m_barColor = QColor(QStringLiteral("#34C759"));
    QColor m_backgroundColor = QColor(QStringLiteral("#2A2E38"));
    bool m_showValue = true;

    // 运行态（不持久化）：最近一次注入的值与质量。
    QVariant m_value;
    Quality m_quality = Quality::Disconnected;
};
