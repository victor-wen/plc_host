#pragma once

#include <QJsonObject>
#include <QPixmap>
#include <QString>

#include "dashboard/DashboardBaseItem.h"

// 图片组件 (docs/superpowers/plans/2026-08-06-plc-host-phase2-dashboard.md
// Task DASH-05)：paint 渲染 QPixmap，按 fitMode 保持比例或填满裁剪。
//
// 业务属性 config["imagePath"]（图片文件路径）、config["fitMode"]
// （"cover"=填满居中裁剪 / "contain"=等比缩放完整显示，默认 "contain"）。
// 图片加载失败时渲染灰色占位框，不阻塞场景。
class ImageItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit ImageItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 图片路径（config["imagePath"] 的便捷访问）。
    QString imagePath() const;
    void setImagePath(const QString& path);

    // 自适应模式：cover/contain。
    QString fitMode() const;
    void setFitMode(const QString& mode);

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

private:
    // 按需加载并缓存 QPixmap（paint 内只读，mutable 缓存）。
    QPixmap pixmap() const;

    QString m_imagePath;
    QString m_fitMode = QStringLiteral("contain");

    mutable QString m_cachePath;
    mutable QPixmap m_pixmap;
};
