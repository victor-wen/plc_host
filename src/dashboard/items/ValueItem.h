#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariant>

#include "dashboard/DashboardBaseItem.h"
#include "domain/TagValue.h"

// 数值显示组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-05)：paint 显示 tag 的工程值，按质量着色。
//
// 业务属性 config["tagId"]、config["precision"]（小数位，默认 1）、
// config["prefix"]、config["suffix"]。运行值通过 setTagValue 注入（TagCache
// 集成在 DASH-09+），未取值时默认显示 "--"（Disconnected 灰色）。
// 质量颜色（冻结）：Good=#34C759 Stale=#FF9500 Bad=#FF3B30 Disconnected=#8E8E93。
class ValueItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit ValueItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 注入运行值。未设置（value 无效）时显示 "--"。
    void setTagValue(const QVariant& value, Quality quality);
    bool hasValue() const;

    // 当前格式化显示文本（未取值时为 "--"）。
    QString displayText() const;

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

private:
    static QColor qualityColor(Quality quality);

    int m_tagId = -1;
    int m_precision = 1;
    QString m_prefix;
    QString m_suffix;

    // 运行态（不持久化）：最近一次注入的值与质量。
    QVariant m_value;
    Quality m_quality = Quality::Disconnected;
};
