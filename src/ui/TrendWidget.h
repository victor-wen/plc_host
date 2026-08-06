#pragma once

#include <QVector>
#include <QWidget>

#include "domain/Tag.h"
#include "domain/TagValue.h"

class QChartView;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class TrendService;

// 趋势页面 (MON-03)：左侧已订阅 tag 列表（checkbox 切换显示/隐藏曲线），
// 右侧 QChartView 展示 TrendService 的实时多曲线，底部时间范围选择与
// "导出数据" 按钮（调用 CsvExporter 导出当前窗口内各曲线的数据）。
class TrendWidget : public QWidget {
    Q_OBJECT
public:
    explicit TrendWidget(QWidget* parent = nullptr);
    ~TrendWidget() override;

    // 设置可订阅的 tag 列表（同步填充左侧列表，默认全部未订阅）。
    void setTags(const QVector<Tag>& tags);

    TrendService* service() const;

public slots:
    // 实时采样入口（引擎 snapshot 需逐条转发到这里）。
    void onTagValueUpdated(const TagValue& tv);

    // 弹出保存对话框并导出。
    void exportData();

    // 不经对话框直接导出到指定路径（测试/自动化用）。
    bool exportDataToFile(const QString& filePath);

private slots:
    void onItemChanged(QListWidgetItem* item);
    void onRangeChanged(int index);

private:
    int currentHistorySeconds() const;

    TrendService* m_service = nullptr;
    QChartView* m_chartView = nullptr;
    QListWidget* m_tagList = nullptr;
    QComboBox* m_rangeCombo = nullptr;
    QPushButton* m_exportButton = nullptr;
    QVector<Tag> m_tags;
    bool m_populating = false;
};
