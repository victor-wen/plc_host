#pragma once

#include <QAbstractTableModel>
#include <QTableView>
#include <QVector>
#include <QWidget>

#include "domain/Tag.h"

class AcquisitionEngine;
class QLabel;
class QMouseEvent;

// Tag 编辑表列定义（顺序即界面显示顺序，CORE-09）。
enum class TagColumn : int {
    Name = 0,
    RegisterType,
    Address,
    DataType,
    ByteOrder,
    WordOrder,
    Scale,
    Offset,
    Unit,
    ReadOnly,
    PollInterval,
    HistoryEnabled,
    Count
};

// Tag 编辑表格模型：持有可变 Tag 列表，支持文本/数字/复选/枚举下拉编辑。
// 线程：UI 线程。变更后发出 modified()（由 widget 转发为 tagsChanged）。
class TagEditorModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit TagEditorModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // 枚举/周期名称（供显示与下拉编辑器共用）。
    static QString registerTypeName(RegisterType type);
    static QString dataTypeName(DataType type);
    static QString byteOrderName(ByteOrder order);
    static QString wordOrderName(WordOrder order);
    static QStringList registerTypeNames();
    static QStringList dataTypeNames();
    static QStringList byteOrderNames();
    static QStringList wordOrderNames();
    static QList<int> pollIntervals();

    QVector<Tag> tags() const;
    void setTags(const QVector<Tag>& tags);
    Tag tagAt(int row) const;
    int addTag();
    int appendTag(const Tag& tag);      // 追加并重新分配 id，返回行号
    bool removeTag(int row);
    bool moveTag(int row, int delta);   // delta=±1：上移/下移

signals:
    void modified();

private:
    int nextId();

    QVector<Tag> m_tags;
    int m_nextId = 1;
};

// 名称列双击触发手动写入（不进入文本编辑）；其余列保持 QTableView 默认编辑行为。
class TagWriteView : public QTableView {
    Q_OBJECT
public:
    using QTableView::QTableView;

signals:
    void writeValueRequested(int row);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};

// Tag 编辑器（CORE-09）：表格 + 工具栏（新增/删除/上移/下移/导入CSV/导出CSV/写入值）。
// 手动写入：双击名称列或“写入值”按钮 → 类型/范围校验 → 确认 → 经引擎入队。
// 线程：UI 线程；调用引擎槽一律走 QMetaObject::invokeMethod(..., Qt::QueuedConnection)。
class TagEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit TagEditorWidget(QWidget* parent = nullptr);

    void setEngine(AcquisitionEngine* engine);
    void setTags(const QVector<Tag>& tags);
    QVector<Tag> tags() const;
    TagEditorModel* model() const;

signals:
    void tagsChanged(const QVector<Tag>& tags);

private slots:
    void onWriteCompleted(int tagId, bool success, const QString& error);

private:
    void addTag();
    void removeSelected();
    void moveUp();
    void moveDown();
    void importCsv();
    void exportCsv();
    void writeValue(int row);
    void emitTagsChanged();
    void setStatus(const QString& text);

    TagEditorModel* m_model = nullptr;
    TagWriteView* m_view = nullptr;
    QLabel* m_statusLabel = nullptr;
    AcquisitionEngine* m_engine = nullptr;
};
