#include "dashboard/items/ImageItem.h"

#include <QColor>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ImageItem::ImageItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("image");
}

QRectF ImageItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

QPixmap ImageItem::pixmap() const
{
    if (m_imagePath.isEmpty())
        return {};
    if (m_cachePath != m_imagePath) {
        m_pixmap = QPixmap(m_imagePath);
        m_cachePath = m_imagePath;
    }
    return m_pixmap;
}

void ImageItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF rect = boundingRect();
    const QPixmap pm = pixmap();
    if (pm.isNull()) {
        // 无路径或加载失败：灰色占位框，不阻塞场景。
        painter->setBrush(QColor(0x2A, 0x2E, 0x38));
        painter->setPen(QPen(QColor(0x5A, 0x64, 0x75), 1, Qt::DashLine));
        painter->drawRect(rect);
        painter->setPen(QColor(0xE0, 0xE3, 0xEA));
        painter->drawText(rect, Qt::AlignCenter, QStringLiteral("image"));
        return;
    }

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (m_fitMode == QLatin1String("cover")) {
        // 等比放大填满目标，超出部分居中裁剪。
        const qreal scale = qMax(rect.width() / pm.width(), rect.height() / pm.height());
        const QSizeF scaled(pm.width() * scale, pm.height() * scale);
        const QRectF source((scaled.width() - rect.width()) / 2.0 / scale,
                            (scaled.height() - rect.height()) / 2.0 / scale,
                            rect.width() / scale, rect.height() / scale);
        painter->drawPixmap(rect, pm, source);
    } else {
        // contain：等比缩放完整显示，居中。
        const qreal scale = qMin(rect.width() / pm.width(), rect.height() / pm.height());
        const QSizeF dest(pm.width() * scale, pm.height() * scale);
        const QRectF target(rect.center().x() - dest.width() / 2.0,
                            rect.center().y() - dest.height() / 2.0,
                            dest.width(), dest.height());
        painter->drawPixmap(target, pm, QRectF());
    }
}

QString ImageItem::imagePath() const
{
    return m_imagePath;
}

void ImageItem::setImagePath(const QString& path)
{
    if (m_imagePath == path)
        return;
    m_imagePath = path;
    config.insert(QStringLiteral("imagePath"), path);
    m_cachePath.clear(); // 强制重新加载
    update();
}

QString ImageItem::fitMode() const
{
    return m_fitMode;
}

void ImageItem::setFitMode(const QString& mode)
{
    if (m_fitMode == mode)
        return;
    m_fitMode = mode;
    config.insert(QStringLiteral("fitMode"), mode);
    update();
}

QJsonObject ImageItem::serialize() const
{
    QJsonObject cfg = config;
    cfg.insert(QStringLiteral("imagePath"), m_imagePath);
    cfg.insert(QStringLiteral("fitMode"), m_fitMode);
    return cfg;
}

void ImageItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    m_imagePath = cfg.value(QStringLiteral("imagePath")).toString();
    m_fitMode = cfg.value(QStringLiteral("fitMode"))
                    .toString(QStringLiteral("contain"));
    m_cachePath.clear();
}
