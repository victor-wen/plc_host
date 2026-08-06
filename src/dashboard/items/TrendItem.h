#pragma once

#include <QJsonObject>

#include "dashboard/DashboardBaseItem.h"

// 趋势区域组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-06)：DASH-06 阶段仅做视觉占位 —— paint 绘制坐标轴与网格，并居中
// 显示 "Trend" 提示。实时曲线数据连接在 MON-03 趋势服务完成后实现
// (docs/superpowers/plans/2026-08-06-plc-host-phase3-monitoring.md)。
//
// 业务属性 config["tagId"]、config["historySeconds"]（历史窗口秒数，默认 60）。
class TrendItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit TrendItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 历史窗口秒数（config["historySeconds"] 的便捷访问）。
    int historySeconds() const;

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

private:
    int m_tagId = -1;
    int m_historySeconds = 60;
};
