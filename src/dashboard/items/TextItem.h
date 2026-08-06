#pragma once

#include <QJsonObject>
#include <QString>

#include "dashboard/DashboardBaseItem.h"

// 文本组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-05)：paint 渲染单行居中文字。
//
// 业务属性 config["text"]；表现层属性 commonStyle["fontSize"]（默认 12px，
// design-tokens fonts.sizes.body）、commonStyle["color"]（默认 #17212B，
// colors.text.primary）。
class TextItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit TextItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 文本内容（config["text"] 的便捷访问；setText 同步写入 config）。
    QString text() const;
    void setText(const QString& text);

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

private:
    QString m_text = QStringLiteral("Text");
};
