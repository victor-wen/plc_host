# Qt Charts：曲线和实时更新

> Qt 版本：6.8（官方页面当前显示的补丁版本为 6.8.8）
> 拉取日期：2026-08-06
> 来源：<https://doc.qt.io/qt-6.8/qtcharts-index.html>、<https://doc.qt.io/qt-6.8/qtcharts-overview.html>、<https://doc.qt.io/qt-6.8/qchart-qtcharts.html>、<https://doc.qt.io/qt-6.8/qchartview-qtcharts.html>、<https://doc.qt.io/qt-6.8/qlineseries-qtcharts.html>、<https://doc.qt.io/qt-6.8/qxyseries-qtcharts.html>、<https://doc.qt.io/qt-6.8/qvalueaxis-qtcharts.html>、<https://doc.qt.io/qt-6.8/qdatetimeaxis-qtcharts.html>、<https://doc.qt.io/qt-6.8/qtcharts-datetimeaxis-example.html>、<https://doc.qt.io/qt-6.8/threads-qobject.html>
> Context7 libraryId：`/websites/doc_qt_io_qt-6`

## 模块和对象关系

Qt Charts 6.8 使用 Graphics View Framework。模块处于维护阶段；本项目因技术基线使用它，不应把 Qt Graphs API 混入 Qt Charts：

```cpp
find_package(Qt6 REQUIRED COMPONENTS Charts)
target_link_libraries(mytarget PRIVATE Qt6::Charts)
```

- `QChart` 继承 `QGraphicsWidget`，管理 series、legend 和 axes，可直接放入 `QGraphicsScene`。
- `QChartView` 继承 `QGraphicsView`，是最方便的 QWidget 容器，不需要手工创建 scene。
- `QLineSeries` 继承 `QXYSeries`，以 `(x, y)` 点组成折线。
- `QValueAxis` 显示数值轴；`QDateTimeAxis` 显示日期时间轴。

使用 Charts 的 QWidget 应用使用 `QApplication`；Qt 文档指出 Charts 的 Graphics View 依赖使 `QGuiApplication` 不足以承载其 QML 类型。

## QChart 和 QChartView

```cpp
QChart::QChart(QGraphicsItem *parent = nullptr,
               Qt::WindowFlags wFlags = Qt::WindowFlags());
void addSeries(QAbstractSeries *series);
void addAxis(QAbstractAxis *axis, Qt::Alignment alignment);
void createDefaultAxes();
QList<QAbstractSeries *> series() const;
QList<QAbstractAxis *> axes(Qt::Orientations orientation =
                            Qt::Horizontal | Qt::Vertical,
                            QAbstractSeries *series = nullptr) const;
void setAnimationOptions(QChart::AnimationOptions options);
void scroll(qreal dx, qreal dy);
void zoom(qreal factor);
void zoomIn();
void zoomOut();
void zoomReset();
```

`QChart` 对通过 `addSeries()` 和 `addAxis()` 添加的对象拥有所有权。`createDefaultAxes()` 会根据**已经添加**的 series 创建轴，并删除先前添加的轴；必须在添加全部 series 后调用。之后再添加的 series 不会自动绑定到既有轴，应显式 `attachAxis()` 或重新创建轴。

动画选项：

```cpp
enum AnimationOption {
    NoAnimation,
    GridAxisAnimations,
    SeriesAnimations,
    AllAnimations
};
```

实时图表应显式关闭动画：

```cpp
chart->setAnimationOptions(QChart::NoAnimation);
```

`QChartView` 常用 API：

```cpp
QChartView::QChartView(QWidget *parent = nullptr);
QChartView::QChartView(QChart *chart, QWidget *parent = nullptr);
QChart *chart() const;
void setChart(QChart *chart);
void setRubberBand(QChartView::RubberBands rubberBand);
```

把 `QChart *` 传给 `QChartView` 构造函数会转移所有权；`setChart()` 会接管新 chart 并释放旧 chart 的所有权，旧对象若不再由其他 owner 管理应显式删除。橡皮筋类型包括 `NoRubberBand`、`VerticalRubberBand`、`HorizontalRubberBand`、`RectangleRubberBand` 和可与其 OR 的 `ClickThroughRubberBand`。

## QLineSeries 和 QXYSeries

```cpp
QLineSeries::QLineSeries(QObject *parent = nullptr);
void append(qreal x, qreal y);
void append(const QPointF &point);
void append(const QList<QPointF> &points);
void clear();
void remove(int index);
void removePoints(int index, int count);
void replace(const QList<QPointF> &points);
int count() const;
QList<QPointF> points() const;
```

`QLineSeries` 连接连续点绘制直线；添加到 `QChart`/`QChartView` 后由 chart/view 管理其生命周期。批量追加可使用 `append(QList<QPointF>)`，批量替换时官方文档指出 `replace(QList<QPointF>)` 比逐点替换或先 clear 再 append 更快。

数据变化信号来自 `QXYSeries`，包括：

```cpp
void pointAdded(int index);
void pointRemoved(int index);
void pointsRemoved(int index, int count);
void pointReplaced(int index);
void pointsReplaced();
```

实时刷新应由一个节拍明确的 UI 定时器或 UI 线程槽驱动，不要让每一个采集线程样本直接改 series。

## 轴

### QValueAxis

```cpp
QValueAxis::QValueAxis(QObject *parent = nullptr);
qreal min() const;
qreal max() const;
void setMin(qreal min);
void setMax(qreal max);
void setRange(qreal min, qreal max);
int tickCount() const;
void setTickCount(int count);
void setLabelFormat(const QString &format);
void applyNiceNumbers();
```

`setRange(min, max)` 在 `min > max` 时不改变范围；`tickCount` 默认 5 且不能小于 2。`labelFormat` 使用 C++ `printf` 风格转换符（如 `%i`、`%.2f`）。`applyNiceNumbers()` 会按 `1/2/5 × 10^n` 调整范围和刻度，使坐标更易读。

### QDateTimeAxis

```cpp
QDateTimeAxis::QDateTimeAxis(QObject *parent = nullptr);
QDateTime min() const;
QDateTime max() const;
void setMin(const QDateTime &min);
void setMax(const QDateTime &max);
void setRange(const QDateTime &min, const QDateTime &max);
QString format() const;
void setFormat(const QString &format);
int tickCount() const;
void setTickCount(int count);
```

`QDateTimeAxis` 的 x 值仍是 `QLineSeries` 的 `qreal`，要使用 `QDateTime` 的毫秒时间戳：

```cpp
const auto now = QDateTime::currentDateTimeUtc();
series->append(now.toMSecsSinceEpoch(), value);

auto *axisX = new QDateTimeAxis;
axisX->setFormat(QStringLiteral("HH:mm:ss"));
axisX->setTickCount(6);
chart->addAxis(axisX, Qt::AlignBottom);
series->attachAxis(axisX);
```

官方示例使用 `QDateTime::toMSecsSinceEpoch()` 添加点，再手动添加 `QDateTimeAxis`；因为 `createDefaultAxes()` 对 `QLineSeries` 默认创建的是 `QValueAxis`。

## 实时更新模式

推荐的固定窗口算法如下：

```cpp
void TrendWidget::appendSample(qint64 timestampMs, qreal value)
{
    m_series->append(static_cast<qreal>(timestampMs), value);

    const int excess = m_series->count() - m_maxPoints;
    if (excess > 0)
        m_series->removePoints(0, excess);

    const qint64 left = timestampMs - m_windowMs;
    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(left, Qt::UTC),
                      QDateTime::fromMSecsSinceEpoch(timestampMs, Qt::UTC));
}
```

实现时遵守：

1. 由 UI 线程批量消费采集快照，每次更新可追加一批 `QList<QPointF>`，减少重绘次数。
2. 用 `removePoints(0, excess)` 限制点数；必须保留窗口上限，防止运行数小时后内存和布局时间无限增长。
3. 对实时曲线使用 `QChart::NoAnimation`；否则每个点都会加入动画队列，造成延迟和堆积。
4. 数值轴用 `QValueAxis::setRange()`；时间轴用 `QDateTimeAxis::setRange()`。自动范围只适合低频或历史图，不适合高频实时窗口。
5. 采集周期、刷新周期和绘制点数独立配置；UI 刷新落后时丢弃中间快照，但保留最新值，不要在 UI 线程追赶无界队列。

## CSV 导出

Charts 不负责 CSV 文件格式。导出时从领域层或 `series->points()` 读取数据，使用 `QFile`/`QTextStream` 写表头和 ISO 时间，不从绘制像素反向读取：

```cpp
bool exportCsv(const QLineSeries *series, const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "timestamp,value\n";
    for (const QPointF &point : series->points()) {
        const auto time = QDateTime::fromMSecsSinceEpoch(
            static_cast<qint64>(point.x()), Qt::UTC);
        out << time.toString(Qt::ISODateWithMs) << ','
            << QString::number(point.y(), 'g', 17) << '\n';
    }
    return true;
}
```

历史导出应直接查询数据库服务，避免把已经裁剪的实时 series 当成完整历史数据。

## 线程安全约束

`QChart`、`QChartView`、`QLineSeries`、轴对象和所有 Graphics View 对象在 UI 主线程创建、修改和销毁。Charts 的 series 不是跨线程共享容器。

通信/数据库线程只产生值类型采样数据，通过信号/槽（必要时 `Qt::QueuedConnection`）或 `QMetaObject::invokeMethod()` 交给 UI 线程；UI 线程再调用 `append()`、`removePoints()` 和 `setRange()`。不要从工作线程直接操作 series、chart 或 view，也不要跨线程传递仍由 chart 拥有的对象指针。
