#pragma once

#include <QGraphicsView>

class QGraphicsScene;

// 看板视图 (docs/qt/qt-widgets-graphics-view.md, Phase 2 DASH-02)
//
// - 鼠标滚轮缩放（以鼠标位置为锚点），缩放范围 0.2x ~ 8x。
// - setEditMode(true) → RubberBandDrag 橡皮筋多选；运行模式 → NoDrag。
// - fitToScreen() 将整个场景矩形适配到视口。
class DashboardView : public QGraphicsView {
    Q_OBJECT
public:
    explicit DashboardView(QGraphicsScene* scene = nullptr, QWidget* parent = nullptr);

    // 切换编辑/运行模式：编辑模式启用橡皮筋多选，运行模式关闭拖拽。
    void setEditMode(bool editing);

    // 自适应窗口：缩放使场景矩形完整可见。
    void fitToScreen();

    // 当前累计缩放因子（1.0 = 原始大小）。
    qreal zoomFactor() const { return m_zoomFactor; }

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    static constexpr qreal kZoomStep = 1.25; // 每格滚轮缩放步进
    static constexpr qreal kMinZoom = 0.2;
    static constexpr qreal kMaxZoom = 8.0;

    qreal m_zoomFactor = 1.0;
};
