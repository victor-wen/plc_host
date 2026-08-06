# Qt Widgets：Graphics View 图形视图框架

> Qt 版本：6.8（官方页面当前显示的补丁版本为 6.8.8）
> 拉取日期：2026-08-06
> 来源：<https://doc.qt.io/qt-6.8/graphicsview.html>、<https://doc.qt.io/qt-6.8/qgraphicsscene.html>、<https://doc.qt.io/qt-6.8/qgraphicsview.html>、<https://doc.qt.io/qt-6.8/qgraphicsobject.html>、<https://doc.qt.io/qt-6.8/qgraphicsitem.html>、<https://doc.qt.io/qt-6.8/threads-qobject.html>
> Context7 libraryId：`/websites/doc_qt_io_qt-6`

## 架构和坐标系

Graphics View 由三层组成：`QGraphicsScene` 管理项和事件，`QGraphicsView` 将场景显示在可滚动视口中，`QGraphicsItem`/`QGraphicsObject` 表示实际图形。多个 view 可以观察同一个 scene；scene 本身没有可见外观，必须通过 view 或 `render()` 输出。

有三个有效坐标系：

1. **项坐标（item coordinates）**：自定义项的本地坐标。`boundingRect()`、`shape()`、`paint()` 和大多数项函数都使用它；通常围绕 `(0, 0)` 设计。
2. **场景坐标（scene coordinates）**：所有顶层项的位置和场景事件使用的坐标。父项的变换会累积到子项的场景位置。
3. **视图坐标（view coordinates）**：viewport 小部件坐标，每单位通常对应一个像素，左上角为 `(0, 0)`。

Graphics View 使用 Qt 坐标约定，Y 轴向下增长；不支持以 Y 轴向上增长的笛卡尔坐标系。常用映射函数为 `QGraphicsView::mapToScene()`/`mapFromScene()` 和 `QGraphicsItem::mapToScene()`/`mapFromScene()`。不要在自定义项中手动叠加 view 的缩放矩阵，view 会在调用 `paint()` 前设置 painter 的变换。

## QGraphicsScene

### 管理项、选择和事件

```cpp
QGraphicsScene::QGraphicsScene(QObject *parent = nullptr);
QGraphicsScene::QGraphicsScene(const QRectF &sceneRect,
                               QObject *parent = nullptr);

void addItem(QGraphicsItem *item);
void removeItem(QGraphicsItem *item);
QList<QGraphicsItem *> items(Qt::SortOrder order = Qt::DescendingOrder) const;
QGraphicsItem *itemAt(const QPointF &position,
                       const QTransform &deviceTransform) const;
QRectF itemsBoundingRect() const;
void render(QPainter *painter, const QRectF &target = QRectF(),
            const QRectF &source = QRectF(),
            Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio);
```

`addItem()` 之后 scene 接管项的所有权；从其他 scene 添加时会先移除再加入。销毁 scene 会移除并删除其中的项。`items()` 可以按点、矩形、路径等查询，返回结果按堆叠顺序排列，第一项在最上层；`itemAt()` 返回指定位置的顶层项。

scene 负责把 view 的鼠标、键盘、拖放和 hover 事件传播给项，也负责焦点和选择：

```cpp
void setSelectionArea(const QPainterPath &path,
                      Qt::ItemSelectionOperation selectionOperation = Qt::ReplaceSelection,
                      Qt::ItemSelectionMode mode = Qt::IntersectsItemShape,
                      const QTransform &deviceTransform = QTransform());
QList<QGraphicsItem *> selectedItems() const;
void clearSelection();
void setFocusItem(QGraphicsItem *item,
                  Qt::FocusReason focusReason = Qt::OtherFocusReason);
QGraphicsItem *focusItem() const;
```

鼠标按下后接受鼠标事件的项成为 mouse grabber，后续鼠标事件会继续发给它，直到最后一个按钮释放。键盘事件发给 focus item；项要获得键盘焦点必须设置相应 flag 并调用 `setFocus()`。

### 场景矩形和索引

`setSceneRect()` 定义场景范围，并用于 view 的滚动条范围和 scene 的索引管理。若未设置，scene 会使用自创建以来所有项的边界；`itemsBoundingRect()` 需要遍历所有项，在大场景中应显式设置 scene rect。

索引算法：

```cpp
enum ItemIndexMethod {
    BspTreeIndex,
    NoIndex
};
```

默认 `BspTreeIndex` 使用 BSP 树，适合大多数项静止的场景，查询接近对数复杂度；`NoIndex` 不维护索引，查询为线性复杂度，但添加、移动、删除是常数级，适合大量动态项。可通过 `setItemIndexMethod()` 切换。`bspTreeDepth` 会影响内存和分区粒度，只有在 scene rect 稳定时才应固定。

## QGraphicsView

`QGraphicsView` 继承 `QAbstractScrollArea`，拥有 viewport 和滚动条；构造时可绑定 scene：

```cpp
QGraphicsView::QGraphicsView(QWidget *parent = nullptr);
QGraphicsView::QGraphicsView(QGraphicsScene *scene,
                             QWidget *parent = nullptr);
void setScene(QGraphicsScene *scene);
QGraphicsScene *scene() const;
void setSceneRect(const QRectF &rect);
void centerOn(const QPointF &pos);
void ensureVisible(const QRectF &rect, int xmargin = 50,
                   int ymargin = 50);
```

### 变换、视口和渲染

```cpp
void setTransform(const QTransform &matrix, bool combine = false);
QTransform transform() const;
void resetTransform();
void scale(qreal sx, qreal sy);
void rotate(qreal angle);
void translate(qreal dx, qreal dy);
QPointF mapToScene(const QPoint &point) const;
QPoint mapFromScene(const QPointF &point) const;
```

缩放、旋转和平移都是 view 到 scene 的仿射变换；默认 `transformationAnchor` 是 `AnchorViewCenter`，变换时保持 view 中心的 scene 点。需要以鼠标为中心缩放时可设置 `AnchorUnderMouse`。`setRenderHints()` 的 hint 会在每个可见项绘制前初始化 `QPainter`，例如：

```cpp
view.setRenderHints(QPainter::Antialiasing |
                    QPainter::SmoothPixmapTransform);
```

`dragMode` 常用值为 `NoDrag`、`ScrollHandDrag`、`RubberBandDrag`；默认是 `NoDrag`。橡皮筋选择要求 view 可交互。`setInteractive(false)` 会把 view 变成只读视图，忽略鼠标和键盘事件。

视图缓存和更新策略要区分：

- `QGraphicsView::CacheNone`：直接绘制到 viewport；默认值。
- `QGraphicsView::CacheBackground`：缓存整个 viewport 背景；变换时会失效，滚动时只需部分失效。
- `MinimalViewportUpdate` 是默认更新模式；动态项很多时可评估 `BoundingRectViewportUpdate` 或 `FullViewportUpdate`。
- `DontAdjustForAntialiasing` 可能减少重绘区域，但抗锯齿边缘移动时可能留下绘制痕迹；不能未经测试全局打开。

可用 `setViewport(new QOpenGLWidget)` 让 view 使用 OpenGL viewport，但会限制部分 `QGraphicsProxyWidget` 和样式组合，必须在目标平台测试。

## QGraphicsObject 和自定义项

`QGraphicsObject` 同时继承 `QObject` 和 `QGraphicsItem`，为项提供 `Q_OBJECT`、属性、信号和槽。常用信号包括 `xChanged()`、`yChanged()`、`rotationChanged()`、`scaleChanged()`、`visibleChanged()`、`opacityChanged()` 和 `zChanged()`。它适合需要动画、属性绑定或事件通知的看板项。

```cpp
class DashboardItem final : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit DashboardItem(QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;
};
```

虽然 `QGraphicsObject` 是 `QObject`，父子关系仍应使用 `QGraphicsItem::setParentItem()`、`parentItem()` 和 `childItems()` 管理，不要混用 `QObject::setParent()` 建立两套不一致的树。`setPos()`、`setRotation()`、`setScale()`、`setTransform()` 和 `setTransformOriginPoint()` 改变项变换；`setZValue()` 控制同级项的层级。

### `boundingRect()`、`shape()`、`paint()` 职责

- `boundingRect()` 是纯虚函数，返回项绘制区域的保守估计。scene 索引、view 裁剪、重绘区域和碰撞快速裁剪都依赖它；笔宽和抗锯齿外扩应包含在矩形内。
- `shape()` 默认可基于 bounding rect，但精确交互/碰撞时应返回本地坐标中的 `QPainterPath`。
- 改变会影响 `boundingRect()` 或 `shape()` 的几何属性前，必须先调用 `prepareGeometryChange()`，让 scene 更新索引。
- `paint()` 只绘制项内容，没有默认背景；未绘制区域会透出后面的项。`update()` 用于安排重绘，不能把它当成 `QWidget::repaint()`。

绘制函数接收的 painter 已在项坐标系中工作：

```cpp
void DashboardItem::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(Qt::white, 1.0));
    painter->setBrush(Qt::darkGray);
    painter->drawRoundedRect(boundingRect(), 4.0, 4.0);
}
```

自定义绘制不能依赖 scene/view 像素坐标，也不应在 `paint()` 中改变项几何或触发递归更新。`QGraphicsView` 会按父项到子项、按递增堆叠顺序绘制；低 `zValue` 先绘制，高 `zValue` 后绘制。默认 `zValue` 为 0，同值的同级项按插入顺序排列；父项总是在子项前绘制。

### flags 和 `itemChange()`

常用 flags 包括 `ItemIsMovable`、`ItemIsSelectable`、`ItemIsFocusable`、`ItemSendsGeometryChanges`、`ItemClipsToShape` 和 `ItemStacksBehindParent`。如果启用了 `ItemSendsGeometryChanges`，可重写：

```cpp
QVariant itemChange(QGraphicsItem::GraphicsItemChange change,
                    const QVariant &value) override;
```

在 `ItemPositionChange`、`ItemTransformChange` 等“即将改变”通知中，不要再调用对应的 `setPos()`/`setTransform()`，否则会递归；应返回调整后的 `QVariant`。`ItemPositionHasChanged` 等“已经改变”通知的返回值会被忽略，适合发业务信号或安排保存。

### 项缓存模式

```cpp
enum CacheMode {
    NoCache,
    ItemCoordinateCache,
    DeviceCoordinateCache
};

void setCacheMode(QGraphicsItem::CacheMode mode,
                  const QSize &logicalCacheSize = QSize());
```

- `NoCache` 是默认值，每次需要重绘时直接调用 `paint()`。
- `ItemCoordinateCache` 在项本地坐标中生成离屏缓存，项变换时可复用，但缓存分辨率可能降低缩放后的质量；需要用 `logicalCacheSize` 控制分辨率。
- `DeviceCoordinateCache` 在设备坐标中缓存，适合会移动但不旋转、不缩放、不 shear 的项；发生直接或间接变换时缓存会重新生成，且保持最大绘制质量。

不要把 `QGraphicsView::CacheBackground` 和 `QGraphicsItem::DeviceCoordinateCache` 混为一谈：前者只缓存 view 背景，后者缓存单个项。

## 线程安全约束

`QGraphicsScene`、`QGraphicsView`、`QGraphicsItem` 和 `QGraphicsObject` 属于 GUI/Graphics View 对象，创建、事件处理、修改几何和绘制必须在 UI 主线程。它们不是可跨线程共享的模型。

后台采集线程只能生成值类型快照（例如 `QVariant`、数值和时间戳），通过信号/槽或 `QMetaObject::invokeMethod()` 投递到 UI 线程，再在 UI 线程更新项并调用 `update()`。禁止从通信/数据库线程直接调用 `setPos()`、`setData()`、`update()` 或访问 scene 的项列表。
