#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QHash>
#include <QVector>
#include <QWidget>

#include "domain/Tag.h"
#include "domain/TagValue.h"

class AcquisitionEngine;
class QLineEdit;
class QTableView;

// 实时变量表模型（CORE-09）：Tag 元数据 + 最新 TagValue 快照。
// 质量列按状态着色：Good=绿, Stale=黄, Bad=红, Disconnected=灰。
// 支持按名称关键字过滤。线程：UI 线程。
class TagMonitorModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit TagMonitorModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setTags(const QVector<Tag>& tags);
    void updateValues(const QHash<int, TagValue>& values);
    void setFilter(const QString& filter);

    static QString qualityText(Quality quality);
    static QColor qualityColor(Quality quality);

private:
    int visibleToTag(int row) const;
    void rebuildVisible();

    QVector<Tag> m_tags;
    QHash<int, TagValue> m_values;   // tagId → 最新值（无值时 data() 显示 "--"）
    QVector<int> m_visible;          // 过滤后的 tag 下标
    QString m_filter;
};

// 实时变量监视表（CORE-09）：QTableView + 搜索框。
// setEngine() 连接 AcquisitionEngine::tagValuesUpdated 自动刷新（跨线程 QueuedConnection）。
class TagMonitorWidget : public QWidget {
    Q_OBJECT
public:
    explicit TagMonitorWidget(QWidget* parent = nullptr);

    void setTags(const QVector<Tag>& tags);
    void setEngine(AcquisitionEngine* engine);

    TagMonitorModel* model() const;

public slots:
    void onValuesUpdated(const QHash<int, TagValue>& values);

private:
    TagMonitorModel* m_model = nullptr;
    QTableView* m_view = nullptr;
    QLineEdit* m_searchEdit = nullptr;
};
