#include "DashboardView.h"

#include <QWheelEvent>

DashboardView::DashboardView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    // 缩放时保持鼠标下的场景点不动；窗口尺寸变化时保持场景中心。
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setDragMode(QGraphicsView::NoDrag);
}

void DashboardView::setEditMode(bool editing)
{
    // 编辑模式：橡皮筋多选；运行模式：无拖拽（组件已锁定）。
    setDragMode(editing ? QGraphicsView::RubberBandDrag : QGraphicsView::NoDrag);
}

void DashboardView::fitToScreen()
{
    if (!scene())
        return;

    // 先隐藏滚动条再适配：若场景大于视口，可见滚动条会挤占视口，导致
    // 适配后留白（两侧都贴合不上）。适配后恢复原策略，此时内容恰好
    // 贴合视口，按需滚动条不会重新出现。
    const Qt::ScrollBarPolicy hPolicy = horizontalScrollBarPolicy();
    const Qt::ScrollBarPolicy vPolicy = verticalScrollBarPolicy();
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
    setHorizontalScrollBarPolicy(hPolicy);
    setVerticalScrollBarPolicy(vPolicy);

    // 变换只含等比缩放（无旋转/切变），m11 即当前缩放因子。
    m_zoomFactor = transform().m11();
}

void DashboardView::wheelEvent(QWheelEvent* event)
{
    const qreal delta = event->angleDelta().y();
    if (delta == 0) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    const qreal step = (delta > 0) ? kZoomStep : (1.0 / kZoomStep);
    const qreal target = qBound(kMinZoom, m_zoomFactor * step, kMaxZoom);
    if (qFuzzyCompare(target, m_zoomFactor))
        return;

    // 相对当前缩放应用增量，避免浮点累计漂移。
    const qreal applied = target / m_zoomFactor;
    m_zoomFactor = target;
    scale(applied, applied);
    event->accept();
}
