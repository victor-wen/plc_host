#include "ui/TrendWidget.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPushButton>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtGlobal>

#include <iterator>

#include "history/CsvExporter.h"
#include "history/TrendService.h"

namespace {

// 时间范围选项（显示文本 → 秒数）。
struct RangeOption {
    const char* label;
    int seconds;
};

constexpr RangeOption kRangeOptions[] = {
    {"1m", 60},
    {"5m", 300},
    {"15m", 900},
    {"1h", 3600},
    {"6h", 21600},
    {"24h", 86400},
};

} // namespace

TrendWidget::TrendWidget(QWidget* parent)
    : QWidget(parent)
{
    m_service = new TrendService(this);

    m_chartView = new QChartView(m_service->chart(), this);
    m_chartView->setRenderHint(QPainter::Antialiasing, true);

    m_tagList = new QListWidget(this);
    m_tagList->setSelectionMode(QAbstractItemView::NoSelection);

    m_rangeCombo = new QComboBox(this);
    for (const RangeOption& opt : kRangeOptions)
        m_rangeCombo->addItem(QString::fromUtf8(opt.label));
    m_rangeCombo->setCurrentIndex(1);   // 默认 5m，与 subscribe 默认一致

    m_exportButton = new QPushButton(QStringLiteral("导出数据"), this);

    auto* bottomRow = new QHBoxLayout;
    bottomRow->addWidget(new QLabel(QStringLiteral("时间范围："), this));
    bottomRow->addWidget(m_rangeCombo);
    bottomRow->addStretch(1);
    bottomRow->addWidget(m_exportButton);

    auto* rightColumn = new QVBoxLayout;
    rightColumn->addWidget(m_chartView, 1);
    rightColumn->addLayout(bottomRow);

    auto* root = new QHBoxLayout(this);
    root->addWidget(m_tagList);
    root->addLayout(rightColumn, 1);

    connect(m_tagList, &QListWidget::itemChanged,
        this, &TrendWidget::onItemChanged);
    connect(m_rangeCombo, &QComboBox::currentIndexChanged,
        this, &TrendWidget::onRangeChanged);
    connect(m_exportButton, &QPushButton::clicked,
        this, &TrendWidget::exportData);
}

TrendWidget::~TrendWidget() = default;

void TrendWidget::setTags(const QVector<Tag>& tags)
{
    m_tags = tags;
    m_populating = true;
    m_tagList->clear();
    for (const Tag& t : m_tags) {
        auto* item = new QListWidgetItem(t.name, m_tagList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);   // 默认不订阅
        item->setData(Qt::UserRole, t.id);
    }
    m_populating = false;
}

TrendService* TrendWidget::service() const
{
    return m_service;
}

void TrendWidget::onTagValueUpdated(const TagValue& tv)
{
    m_service->onTagValueUpdated(tv);
}

int TrendWidget::currentHistorySeconds() const
{
    const int idx = m_rangeCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(std::size(kRangeOptions)))
        return 300;
    return kRangeOptions[idx].seconds;
}

void TrendWidget::onItemChanged(QListWidgetItem* item)
{
    if (m_populating || item == nullptr)
        return;

    const int tagId = item->data(Qt::UserRole).toInt();
    const bool subscribed = item->checkState() == Qt::Checked;
    if (subscribed) {
        m_service->subscribe(tagId, currentHistorySeconds());
        QLineSeries* series = m_service->seriesFor(tagId);
        if (series != nullptr) {
            for (const Tag& t : m_tags) {
                if (t.id == tagId) {
                    series->setName(t.name);
                    break;
                }
            }
        }
    } else {
        m_service->unsubscribe(tagId);
    }
}

void TrendWidget::onRangeChanged(int index)
{
    Q_UNUSED(index);
    const int seconds = currentHistorySeconds();
    for (int i = 0; i < m_tagList->count(); ++i) {
        QListWidgetItem* item = m_tagList->item(i);
        if (item->checkState() == Qt::Checked)
            m_service->setHistoryWindow(item->data(Qt::UserRole).toInt(), seconds);
    }
}

void TrendWidget::exportData()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出趋势数据"), QStringLiteral("trend_export.csv"),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (filePath.isEmpty())
        return;
    exportDataToFile(filePath);
}

bool TrendWidget::exportDataToFile(const QString& filePath)
{
    // 收集当前各已订阅曲线窗口内的点，转成 TagValue 后由 CsvExporter 导出。
    QVector<TagValue> rows;
    for (const Tag& t : m_tags) {
        QLineSeries* series = m_service->seriesFor(t.id);
        if (series == nullptr)
            continue;
        const QVector<QPointF> pts = series->points();
        for (const QPointF& p : pts) {
            TagValue tv;
            tv.tagId = t.id;
            tv.value = p.y();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            tv.timestamp = QDateTime::fromMSecsSinceEpoch(qint64(p.x()), QTimeZone::UTC);
#else
            tv.timestamp = QDateTime::fromMSecsSinceEpoch(qint64(p.x()), Qt::UTC); // not deprecated before Qt 6.5
#endif
            tv.quality = Quality::Good;
            rows.append(tv);
        }
    }
    return CsvExporter::exportHistory(filePath, rows, m_tags);
}
