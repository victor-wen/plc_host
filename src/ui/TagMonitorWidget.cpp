#include "ui/TagMonitorWidget.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableView>
#include <QVBoxLayout>

#include "runtime/AcquisitionEngine.h"

namespace {

// 监视表列。
enum class MonitorColumn : int {
    Name = 0,
    Value,
    RawValue,
    Quality,
    Timestamp,
    Unit,
    Count
};

} // namespace

TagMonitorModel::TagMonitorModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int TagMonitorModel::rowCount(const QModelIndex&) const
{
    return m_visible.size();
}

int TagMonitorModel::columnCount(const QModelIndex&) const
{
    return static_cast<int>(MonitorColumn::Count);
}

QString TagMonitorModel::qualityText(Quality quality)
{
    switch (quality) {
    case Quality::Good:
        return QStringLiteral("Good");
    case Quality::Stale:
        return QStringLiteral("Stale");
    case Quality::Bad:
        return QStringLiteral("Bad");
    case Quality::Disconnected:
        return QStringLiteral("Disconnected");
    }
    return QStringLiteral("Unknown");
}

QColor TagMonitorModel::qualityColor(Quality quality)
{
    switch (quality) {
    case Quality::Good:
        return QColor(QStringLiteral("#2e9e44"));   // 绿
    case Quality::Stale:
        return QColor(QStringLiteral("#e6a23c"));   // 黄
    case Quality::Bad:
        return QColor(QStringLiteral("#d64541"));   // 红
    case Quality::Disconnected:
        return QColor(QStringLiteral("#999999"));   // 灰
    }
    return QColor(QStringLiteral("#999999"));
}

int TagMonitorModel::visibleToTag(int row) const
{
    if (row < 0 || row >= m_visible.size())
        return -1;
    return m_visible[row];
}

void TagMonitorModel::rebuildVisible()
{
    m_visible.clear();
    for (int i = 0; i < m_tags.size(); ++i) {
        if (m_filter.isEmpty() || m_tags[i].name.contains(m_filter, Qt::CaseInsensitive))
            m_visible.append(i);
    }
}

void TagMonitorModel::setTags(const QVector<Tag>& tags)
{
    beginResetModel();
    m_tags = tags;
    m_values.clear();
    rebuildVisible();
    endResetModel();
}

void TagMonitorModel::updateValues(const QHash<int, TagValue>& values)
{
    if (values.isEmpty() || m_visible.isEmpty())
        return;
    m_values.insert(values);   // 合并覆盖，不丢失未更新 tag
    emit dataChanged(index(0, 0), index(m_visible.size() - 1, columnCount() - 1));
}

void TagMonitorModel::setFilter(const QString& filter)
{
    m_filter = filter.trimmed();
    beginResetModel();
    rebuildVisible();
    endResetModel();
}

QVariant TagMonitorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    const int tagIdx = visibleToTag(index.row());
    if (tagIdx < 0)
        return {};
    const Tag& tag = m_tags[tagIdx];
    const TagValue tv = m_values.value(tag.id);   // 无值 → 默认（quality=Disconnected）

    switch (static_cast<MonitorColumn>(index.column())) {
    case MonitorColumn::Name:
        if (role == Qt::DisplayRole)
            return tag.name;
        break;
    case MonitorColumn::Value:
        if (role == Qt::DisplayRole)
            return tv.value.isValid() ? tv.value.toString() : QStringLiteral("--");
        if (role == Qt::BackgroundRole)
            return qualityColor(tv.quality);
        break;
    case MonitorColumn::RawValue:
        if (role == Qt::DisplayRole)
            return tv.rawValue.isValid() ? tv.rawValue.toString() : QStringLiteral("--");
        break;
    case MonitorColumn::Quality:
        if (role == Qt::DisplayRole)
            return qualityText(tv.quality);
        if (role == Qt::BackgroundRole)
            return qualityColor(tv.quality);
        if (role == Qt::ForegroundRole)
            return QColor(Qt::white);
        break;
    case MonitorColumn::Timestamp:
        if (role == Qt::DisplayRole)
            return tv.timestamp.isValid()
                ? tv.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"))
                : QStringLiteral("--");
        break;
    case MonitorColumn::Unit:
        if (role == Qt::DisplayRole)
            return tag.unit;
        break;
    case MonitorColumn::Count:
        break;
    }
    return {};
}

QVariant TagMonitorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    switch (static_cast<MonitorColumn>(section)) {
    case MonitorColumn::Name:
        return QStringLiteral("名称");
    case MonitorColumn::Value:
        return QStringLiteral("值");
    case MonitorColumn::RawValue:
        return QStringLiteral("原始值");
    case MonitorColumn::Quality:
        return QStringLiteral("质量");
    case MonitorColumn::Timestamp:
        return QStringLiteral("时间戳");
    case MonitorColumn::Unit:
        return QStringLiteral("单位");
    case MonitorColumn::Count:
        break;
    }
    return {};
}

// ---------------------------------------------------------------- 控件

TagMonitorWidget::TagMonitorWidget(QWidget* parent)
    : QWidget(parent)
{
    m_model = new TagMonitorModel(this);

    m_view = new QTableView(this);
    m_view->setModel(m_model);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setAlternatingRowColors(true);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->verticalHeader()->setVisible(false);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("按名称过滤..."));
    m_searchEdit->setClearButtonEnabled(true);

    auto* searchRow = new QHBoxLayout;
    searchRow->addWidget(new QLabel(QStringLiteral("搜索："), this));
    searchRow->addWidget(m_searchEdit, 1);

    auto* root = new QVBoxLayout(this);
    root->addLayout(searchRow);
    root->addWidget(m_view, 1);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_model->setFilter(text);
    });
}

TagMonitorModel* TagMonitorWidget::model() const
{
    return m_model;
}

void TagMonitorWidget::setTags(const QVector<Tag>& tags)
{
    m_model->setTags(tags);
}

void TagMonitorWidget::setEngine(AcquisitionEngine* engine)
{
    if (engine == nullptr)
        return;
    // 引擎在通信线程 emit，经 QueuedConnection 自动转回 UI 线程。
    connect(engine, &AcquisitionEngine::tagValuesUpdated,
        this, &TagMonitorWidget::onValuesUpdated);
}

void TagMonitorWidget::onValuesUpdated(const QHash<int, TagValue>& values)
{
    m_model->updateValues(values);
}
