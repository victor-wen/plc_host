#include "ui/TagEditorWidget.h"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QVBoxLayout>

#include <limits>

#include "runtime/AcquisitionEngine.h"

namespace {

// 枚举下拉编辑器（无 Q_OBJECT，纯工具类，不参与元对象系统）。
class ComboBoxDelegate : public QStyledItemDelegate {
public:
    ComboBoxDelegate(const QStringList& items, const QList<QVariant>& values, QObject* parent)
        : QStyledItemDelegate(parent)
        , m_items(items)
        , m_values(values)
    {
    }

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        auto* combo = new QComboBox(parent);
        combo->addItems(m_items);
        return combo;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        auto* combo = qobject_cast<QComboBox*>(editor);
        if (combo == nullptr)
            return;
        const QVariant current = index.data(Qt::EditRole);
        combo->setCurrentIndex(m_values.indexOf(current));
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        auto* combo = qobject_cast<QComboBox*>(editor);
        if (combo == nullptr)
            return;
        model->setData(index, m_values.value(combo->currentIndex()), Qt::EditRole);
    }

private:
    QStringList m_items;
    QList<QVariant> m_values;
};

QList<QVariant> toVariantList(const QList<int>& values)
{
    QList<QVariant> out;
    out.reserve(values.size());
    for (int v : values)
        out.append(v);
    return out;
}

bool boolFromVariant(const QVariant& value)
{
    return value.toInt() == Qt::Checked;
}

} // namespace

// ---------------------------------------------------------------- 模型

TagEditorModel::TagEditorModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int TagEditorModel::rowCount(const QModelIndex&) const
{
    return m_tags.size();
}

int TagEditorModel::columnCount(const QModelIndex&) const
{
    return static_cast<int>(TagColumn::Count);
}

QString TagEditorModel::registerTypeName(RegisterType type)
{
    const QStringList names = registerTypeNames();
    const int idx = static_cast<int>(type);
    return (idx >= 0 && idx < names.size()) ? names[idx] : QString();
}

QString TagEditorModel::dataTypeName(DataType type)
{
    const QStringList names = dataTypeNames();
    const int idx = static_cast<int>(type);
    return (idx >= 0 && idx < names.size()) ? names[idx] : QString();
}

QString TagEditorModel::byteOrderName(ByteOrder order)
{
    const QStringList names = byteOrderNames();
    const int idx = static_cast<int>(order);
    return (idx >= 0 && idx < names.size()) ? names[idx] : QString();
}

QString TagEditorModel::wordOrderName(WordOrder order)
{
    const QStringList names = wordOrderNames();
    const int idx = static_cast<int>(order);
    return (idx >= 0 && idx < names.size()) ? names[idx] : QString();
}

QStringList TagEditorModel::registerTypeNames()
{
    return {
        QStringLiteral("线圈"),
        QStringLiteral("离散输入"),
        QStringLiteral("输入寄存器"),
        QStringLiteral("保持寄存器"),
    };
}

QStringList TagEditorModel::dataTypeNames()
{
    return {
        QStringLiteral("Bool"),
        QStringLiteral("Int16"),
        QStringLiteral("UInt16"),
        QStringLiteral("Int32"),
        QStringLiteral("UInt32"),
        QStringLiteral("Float32"),
        QStringLiteral("BitField"),
    };
}

QStringList TagEditorModel::byteOrderNames()
{
    return {QStringLiteral("ABCD"), QStringLiteral("DCBA"), QStringLiteral("BADC"), QStringLiteral("CDAB")};
}

QStringList TagEditorModel::wordOrderNames()
{
    return {QStringLiteral("高低字"), QStringLiteral("低高字")};
}

QList<int> TagEditorModel::pollIntervals()
{
    return {200, 500, 1000, 2000, 5000};
}

QVariant TagEditorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tags.size())
        return {};
    const Tag& tag = m_tags[index.row()];
    const auto col = static_cast<TagColumn>(index.column());

    if (role == Qt::DisplayRole) {
        switch (col) {
        case TagColumn::Name:
            return tag.name;
        case TagColumn::RegisterType:
            return registerTypeName(tag.registerType);
        case TagColumn::Address:
            return tag.address;
        case TagColumn::DataType:
            return dataTypeName(tag.dataType);
        case TagColumn::ByteOrder:
            return byteOrderName(tag.byteOrder);
        case TagColumn::WordOrder:
            return wordOrderName(tag.wordOrder);
        case TagColumn::Scale:
            return tag.scale;
        case TagColumn::Offset:
            return tag.offset;
        case TagColumn::Unit:
            return tag.unit;
        case TagColumn::ReadOnly:
        case TagColumn::HistoryEnabled:
            return {};
        case TagColumn::PollInterval:
            return tag.pollIntervalMs;
        case TagColumn::Count:
            break;
        }
        return {};
    }

    if (role == Qt::EditRole) {
        switch (col) {
        case TagColumn::Name:
            return tag.name;
        case TagColumn::RegisterType:
            return static_cast<int>(tag.registerType);
        case TagColumn::Address:
            return tag.address;
        case TagColumn::DataType:
            return static_cast<int>(tag.dataType);
        case TagColumn::ByteOrder:
            return static_cast<int>(tag.byteOrder);
        case TagColumn::WordOrder:
            return static_cast<int>(tag.wordOrder);
        case TagColumn::Scale:
            return tag.scale;
        case TagColumn::Offset:
            return tag.offset;
        case TagColumn::Unit:
            return tag.unit;
        case TagColumn::ReadOnly:
            return tag.readOnly ? Qt::Checked : Qt::Unchecked;
        case TagColumn::PollInterval:
            return tag.pollIntervalMs;
        case TagColumn::HistoryEnabled:
            return tag.historyEnabled ? Qt::Checked : Qt::Unchecked;
        case TagColumn::Count:
            break;
        }
        return {};
    }

    if (role == Qt::CheckStateRole) {
        switch (col) {
        case TagColumn::ReadOnly:
            return tag.readOnly ? Qt::Checked : Qt::Unchecked;
        case TagColumn::HistoryEnabled:
            return tag.historyEnabled ? Qt::Checked : Qt::Unchecked;
        default:
            break;
        }
        return {};
    }

    if (role == Qt::TextAlignmentRole) {
        switch (col) {
        case TagColumn::Address:
        case TagColumn::Scale:
        case TagColumn::Offset:
        case TagColumn::PollInterval:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            break;
        }
        return {};
    }

    return {};
}

bool TagEditorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tags.size())
        return false;
    if (role != Qt::EditRole && role != Qt::CheckStateRole)
        return false;

    Tag& tag = m_tags[index.row()];
    const auto col = static_cast<TagColumn>(index.column());

    switch (col) {
    case TagColumn::Name: {
        const QString name = value.toString().trimmed();
        if (name.isEmpty())
            return false;
        tag.name = name;
        break;
    }
    case TagColumn::RegisterType:
        tag.registerType = static_cast<RegisterType>(value.toInt());
        break;
    case TagColumn::Address: {
        const int address = value.toInt();
        if (address < 0)
            return false;
        tag.address = address;
        break;
    }
    case TagColumn::DataType:
        tag.dataType = static_cast<DataType>(value.toInt());
        break;
    case TagColumn::ByteOrder:
        tag.byteOrder = static_cast<ByteOrder>(value.toInt());
        break;
    case TagColumn::WordOrder:
        tag.wordOrder = static_cast<WordOrder>(value.toInt());
        break;
    case TagColumn::Scale: {
        const double scale = value.toDouble();
        if (scale == 0.0)
            return false;   // scale=0 会导致解码/编码除零（ValueCodec）
        tag.scale = scale;
        break;
    }
    case TagColumn::Offset:
        tag.offset = value.toDouble();
        break;
    case TagColumn::Unit:
        tag.unit = value.toString();
        break;
    case TagColumn::ReadOnly:
        tag.readOnly = boolFromVariant(value);
        break;
    case TagColumn::PollInterval: {
        const int ms = value.toInt();
        if (ms <= 0)
            return false;
        tag.pollIntervalMs = ms;
        break;
    }
    case TagColumn::HistoryEnabled:
        tag.historyEnabled = boolFromVariant(value);
        break;
    case TagColumn::Count:
        return false;
    }

    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole, Qt::CheckStateRole});
    emit modified();
    return true;
}

QVariant TagEditorModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    switch (static_cast<TagColumn>(section)) {
    case TagColumn::Name:
        return QStringLiteral("名称");
    case TagColumn::RegisterType:
        return QStringLiteral("寄存器类型");
    case TagColumn::Address:
        return QStringLiteral("地址");
    case TagColumn::DataType:
        return QStringLiteral("数据类型");
    case TagColumn::ByteOrder:
        return QStringLiteral("字节序");
    case TagColumn::WordOrder:
        return QStringLiteral("字序");
    case TagColumn::Scale:
        return QStringLiteral("倍率");
    case TagColumn::Offset:
        return QStringLiteral("偏移");
    case TagColumn::Unit:
        return QStringLiteral("单位");
    case TagColumn::ReadOnly:
        return QStringLiteral("只读");
    case TagColumn::PollInterval:
        return QStringLiteral("采集周期");
    case TagColumn::HistoryEnabled:
        return QStringLiteral("历史");
    case TagColumn::Count:
        break;
    }
    return {};
}

Qt::ItemFlags TagEditorModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    const auto col = static_cast<TagColumn>(index.column());
    if (col == TagColumn::ReadOnly || col == TagColumn::HistoryEnabled)
        f |= Qt::ItemIsUserCheckable;
    return f;
}

QVector<Tag> TagEditorModel::tags() const
{
    return m_tags;
}

void TagEditorModel::setTags(const QVector<Tag>& tags)
{
    beginResetModel();
    m_tags = tags;
    m_nextId = 1;
    for (const Tag& tag : m_tags)
        m_nextId = std::max(m_nextId, tag.id + 1);
    endResetModel();
}

Tag TagEditorModel::tagAt(int row) const
{
    if (row < 0 || row >= m_tags.size())
        return {};
    return m_tags[row];
}

int TagEditorModel::nextId()
{
    while (true) {
        const int id = m_nextId++;
        bool used = false;
        for (const Tag& tag : m_tags) {
            if (tag.id == id) {
                used = true;
                break;
            }
        }
        if (!used)
            return id;
    }
}

int TagEditorModel::addTag()
{
    const int row = m_tags.size();
    beginInsertRows(QModelIndex(), row, row);
    Tag tag;
    tag.id = nextId();
    tag.name = QStringLiteral("Tag %1").arg(tag.id);
    m_tags.append(tag);
    endInsertRows();
    emit modified();
    return row;
}

int TagEditorModel::appendTag(const Tag& tag)
{
    const int row = m_tags.size();
    beginInsertRows(QModelIndex(), row, row);
    Tag copy = tag;
    copy.id = nextId();   // 外部 id 不可信（CSV/DB 导入），统一重新分配
    m_tags.append(copy);
    endInsertRows();
    emit modified();
    return row;
}

bool TagEditorModel::removeTag(int row)
{
    if (row < 0 || row >= m_tags.size())
        return false;
    beginRemoveRows(QModelIndex(), row, row);
    m_tags.removeAt(row);
    endRemoveRows();
    emit modified();
    return true;
}

bool TagEditorModel::moveTag(int row, int delta)
{
    const int target = row + delta;
    if (row < 0 || row >= m_tags.size() || target < 0 || target >= m_tags.size())
        return false;
    beginMoveRows(QModelIndex(), row, row, QModelIndex(), delta > 0 ? target + 1 : target);
    m_tags.move(row, target);
    endMoveRows();
    emit modified();
    return true;
}

// ---------------------------------------------------------------- 视图

void TagWriteView::mouseDoubleClickEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid() && index.column() == static_cast<int>(TagColumn::Name)) {
        emit writeValueRequested(index.row());
        return;   // 名称列双击 → 手动写入，不进入文本编辑
    }
    QTableView::mouseDoubleClickEvent(event);
}

// ---------------------------------------------------------------- 手动写入校验

namespace {

// 校验用户输入并按 Tag 类型范围收敛为可写入的 QVariant（写前校验，interfaces.md §3）。
bool parseWriteValue(const Tag& tag, const QString& input, QVariant& value, QString& error)
{
    switch (tag.dataType) {
    case DataType::Bool: {
        const QString s = input.trimmed().toLower();
        if (s == QStringLiteral("true") || s == QStringLiteral("1") || s == QStringLiteral("on")) {
            value = true;
            return true;
        }
        if (s == QStringLiteral("false") || s == QStringLiteral("0") || s == QStringLiteral("off")) {
            value = false;
            return true;
        }
        error = QStringLiteral("Bool 类型请输入 true/false 或 1/0");
        return false;
    }
    case DataType::Int16: {
        bool ok = false;
        const int v = input.toInt(&ok);
        if (!ok || v < std::numeric_limits<qint16>::min() || v > std::numeric_limits<qint16>::max()) {
            error = QStringLiteral("Int16 范围 [-32768, 32767]");
            return false;
        }
        value = v;
        return true;
    }
    case DataType::UInt16: {
        bool ok = false;
        const int v = input.toInt(&ok);
        if (!ok || v < 0 || v > std::numeric_limits<quint16>::max()) {
            error = QStringLiteral("UInt16 范围 [0, 65535]");
            return false;
        }
        value = v;
        return true;
    }
    case DataType::Int32: {
        bool ok = false;
        const qint64 v = input.toLongLong(&ok);
        if (!ok || v < std::numeric_limits<qint32>::min() || v > std::numeric_limits<qint32>::max()) {
            error = QStringLiteral("Int32 范围 [-2147483648, 2147483647]");
            return false;
        }
        value = static_cast<int>(v);
        return true;
    }
    case DataType::UInt32: {
        bool ok = false;
        const qint64 v = input.toLongLong(&ok);
        if (!ok || v < 0 || v > static_cast<qint64>(std::numeric_limits<quint32>::max())) {
            error = QStringLiteral("UInt32 范围 [0, 4294967295]");
            return false;
        }
        value = static_cast<quint32>(v);
        return true;
    }
    case DataType::Float32: {
        bool ok = false;
        const double v = input.toDouble(&ok);
        if (!ok || !std::isfinite(v)) {
            error = QStringLiteral("Float32 请输入有效数字");
            return false;
        }
        value = v;
        return true;
    }
    case DataType::BitField: {
        bool ok = false;
        const qint64 v = input.toLongLong(&ok);
        if (!ok || tag.bitLength <= 0 || tag.bitLength > 30) {
            error = QStringLiteral("BitField 位宽配置非法");
            return false;
        }
        const qint64 maxValue = (static_cast<qint64>(1) << tag.bitLength) - 1;
        if (v < 0 || v > maxValue) {
            error = QStringLiteral("BitField(%1 bit) 范围 [0, %2]").arg(tag.bitLength).arg(maxValue);
            return false;
        }
        value = static_cast<int>(v);
        return true;
    }
    }
    error = QStringLiteral("不支持的数据类型");
    return false;
}

// 解析一行 CSV（跳过表头与非法行）。字段：name,registerType,address,dataType,
// byteOrder,wordOrder,scale,offset,unit,readOnly,pollInterval,historyEnabled。
bool parseCsvLine(const QString& line, Tag& tag)
{
    if (line.trimmed().isEmpty())
        return false;
    const QStringList parts = line.split(QLatin1Char(','));
    if (parts.size() < 12)
        return false;

    bool ok = false;
    tag.name = parts[0].trimmed();
    if (tag.name.isEmpty())
        return false;

    const int rt = parts[1].toInt(&ok);
    if (!ok || rt < 0 || rt >= static_cast<int>(RegisterType::HoldingRegister) + 1)
        return false;
    tag.registerType = static_cast<RegisterType>(rt);

    const int address = parts[2].toInt(&ok);
    if (!ok || address < 0)
        return false;
    tag.address = address;

    const int dt = parts[3].toInt(&ok);
    if (!ok || dt < 0 || dt >= static_cast<int>(DataType::BitField) + 1)
        return false;
    tag.dataType = static_cast<DataType>(dt);

    const int bo = parts[4].toInt(&ok);
    if (!ok || bo < 0 || bo >= static_cast<int>(ByteOrder::CDAB) + 1)
        return false;
    tag.byteOrder = static_cast<ByteOrder>(bo);

    const int wo = parts[5].toInt(&ok);
    if (!ok || wo < 0 || wo >= static_cast<int>(WordOrder::LowHigh) + 1)
        return false;
    tag.wordOrder = static_cast<WordOrder>(wo);

    const double scale = parts[6].toDouble(&ok);
    if (!ok || scale == 0.0)
        return false;
    tag.scale = scale;

    const double offset = parts[7].toDouble(&ok);
    if (!ok)
        return false;
    tag.offset = offset;

    tag.unit = parts[8];

    const int readOnly = parts[9].toInt(&ok);
    if (!ok)
        return false;
    tag.readOnly = (readOnly != 0);

    const int interval = parts[10].toInt(&ok);
    if (!ok || interval <= 0)
        return false;
    tag.pollIntervalMs = interval;

    const int history = parts[11].toInt(&ok);
    if (!ok)
        return false;
    tag.historyEnabled = (history != 0);
    return true;
}

QString csvHeader()
{
    return QStringLiteral(
        "name,registerType,address,dataType,byteOrder,wordOrder,scale,offset,unit,readOnly,pollInterval,historyEnabled");
}

QString tagToCsvLine(const Tag& tag)
{
    return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12")
        .arg(tag.name)
        .arg(static_cast<int>(tag.registerType))
        .arg(tag.address)
        .arg(static_cast<int>(tag.dataType))
        .arg(static_cast<int>(tag.byteOrder))
        .arg(static_cast<int>(tag.wordOrder))
        .arg(tag.scale)
        .arg(tag.offset)
        .arg(tag.unit)
        .arg(tag.readOnly ? 1 : 0)
        .arg(tag.pollIntervalMs)
        .arg(tag.historyEnabled ? 1 : 0);
}

} // namespace

// ---------------------------------------------------------------- 控件

TagEditorWidget::TagEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    m_model = new TagEditorModel(this);

    m_view = new TagWriteView(this);
    m_view->setModel(m_model);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setAlternatingRowColors(true);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->verticalHeader()->setVisible(false);

    m_view->setItemDelegateForColumn(static_cast<int>(TagColumn::RegisterType),
        new ComboBoxDelegate(TagEditorModel::registerTypeNames(),
            toVariantList({0, 1, 2, 3}), this));
    m_view->setItemDelegateForColumn(static_cast<int>(TagColumn::DataType),
        new ComboBoxDelegate(TagEditorModel::dataTypeNames(),
            toVariantList({0, 1, 2, 3, 4, 5, 6}), this));
    m_view->setItemDelegateForColumn(static_cast<int>(TagColumn::ByteOrder),
        new ComboBoxDelegate(TagEditorModel::byteOrderNames(),
            toVariantList({0, 1, 2, 3}), this));
    m_view->setItemDelegateForColumn(static_cast<int>(TagColumn::WordOrder),
        new ComboBoxDelegate(TagEditorModel::wordOrderNames(),
            toVariantList({0, 1}), this));
    m_view->setItemDelegateForColumn(static_cast<int>(TagColumn::PollInterval),
        new ComboBoxDelegate({QStringLiteral("200 ms"), QStringLiteral("500 ms"),
                                  QStringLiteral("1000 ms"), QStringLiteral("2000 ms"),
                                  QStringLiteral("5000 ms")},
            toVariantList(TagEditorModel::pollIntervals()), this));

    auto* addBtn = new QPushButton(QStringLiteral("新增"), this);
    auto* delBtn = new QPushButton(QStringLiteral("删除"), this);
    auto* upBtn = new QPushButton(QStringLiteral("上移"), this);
    auto* downBtn = new QPushButton(QStringLiteral("下移"), this);
    auto* importBtn = new QPushButton(QStringLiteral("导入CSV"), this);
    auto* exportBtn = new QPushButton(QStringLiteral("导出CSV"), this);
    auto* writeBtn = new QPushButton(QStringLiteral("写入值"), this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(addBtn);
    btnRow->addWidget(delBtn);
    btnRow->addWidget(upBtn);
    btnRow->addWidget(downBtn);
    btnRow->addWidget(importBtn);
    btnRow->addWidget(exportBtn);
    btnRow->addWidget(writeBtn);
    btnRow->addStretch(1);

    m_statusLabel = new QLabel(this);

    auto* root = new QVBoxLayout(this);
    root->addLayout(btnRow);
    root->addWidget(m_view, 1);
    root->addWidget(m_statusLabel);

    connect(addBtn, &QPushButton::clicked, this, &TagEditorWidget::addTag);
    connect(delBtn, &QPushButton::clicked, this, &TagEditorWidget::removeSelected);
    connect(upBtn, &QPushButton::clicked, this, &TagEditorWidget::moveUp);
    connect(downBtn, &QPushButton::clicked, this, &TagEditorWidget::moveDown);
    connect(importBtn, &QPushButton::clicked, this, &TagEditorWidget::importCsv);
    connect(exportBtn, &QPushButton::clicked, this, &TagEditorWidget::exportCsv);
    connect(writeBtn, &QPushButton::clicked, this, [this]() {
        writeValue(m_view->currentIndex().row());
    });
    connect(m_view, &TagWriteView::writeValueRequested, this, &TagEditorWidget::writeValue);
    connect(m_model, &TagEditorModel::modified, this, &TagEditorWidget::emitTagsChanged);
}

TagEditorModel* TagEditorWidget::model() const
{
    return m_model;
}

QVector<Tag> TagEditorWidget::tags() const
{
    return m_model->tags();
}

void TagEditorWidget::setTags(const QVector<Tag>& tags)
{
    m_model->setTags(tags);
}

void TagEditorWidget::setEngine(AcquisitionEngine* engine)
{
    m_engine = engine;
    if (engine == nullptr)
        return;
    connect(engine, &AcquisitionEngine::writeCompleted,
        this, &TagEditorWidget::onWriteCompleted);
}

void TagEditorWidget::emitTagsChanged()
{
    emit tagsChanged(m_model->tags());
}

void TagEditorWidget::addTag()
{
    const int row = m_model->addTag();
    m_view->selectRow(row);
    m_view->setCurrentIndex(m_model->index(row, static_cast<int>(TagColumn::Name)));
    m_view->edit(m_model->index(row, static_cast<int>(TagColumn::Name)));
    setStatus(QStringLiteral("已新增变量"));
}

void TagEditorWidget::removeSelected()
{
    const int row = m_view->currentIndex().row();
    if (row < 0)
        return;
    const QString name = m_model->tagAt(row).name;
    if (QMessageBox::question(this, QStringLiteral("删除变量"),
            QStringLiteral("确认删除变量 %1？").arg(name)) != QMessageBox::Yes)
        return;
    if (m_model->removeTag(row))
        setStatus(QStringLiteral("已删除 %1").arg(name));
}

void TagEditorWidget::moveUp()
{
    const int row = m_view->currentIndex().row();
    if (m_model->moveTag(row, -1))
        m_view->selectRow(row - 1);
}

void TagEditorWidget::moveDown()
{
    const int row = m_view->currentIndex().row();
    if (m_model->moveTag(row, 1))
        m_view->selectRow(row + 1);
}

void TagEditorWidget::importCsv()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("导入 CSV"),
        QString(), QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), file.errorString());
        return;
    }

    QTextStream in(&file);
    in.readLine();   // 跳过表头

    int imported = 0;
    while (!in.atEnd()) {
        Tag tag;
        if (parseCsvLine(in.readLine(), tag)) {
            m_model->appendTag(tag);
            ++imported;
        }
    }
    setStatus(QStringLiteral("已导入 %1 条记录").arg(imported));
}

void TagEditorWidget::exportCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 CSV"),
        QStringLiteral("tags.csv"), QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }

    QTextStream out(&file);
    out << csvHeader() << '\n';
    for (const Tag& tag : m_model->tags())
        out << tagToCsvLine(tag) << '\n';
    setStatus(QStringLiteral("已导出 %1 条记录").arg(m_model->rowCount()));
}

void TagEditorWidget::writeValue(int row)
{
    if (row < 0 || row >= m_model->rowCount())
        return;
    const Tag tag = m_model->tagAt(row);
    if (tag.readOnly) {
        QMessageBox::information(this, QStringLiteral("写入"),
            QStringLiteral("变量 %1 为只读，不能手动写入").arg(tag.name));
        return;
    }
    if (m_engine == nullptr) {
        QMessageBox::warning(this, QStringLiteral("写入"), QStringLiteral("采集引擎未就绪"));
        return;
    }

    bool ok = false;
    const QString input = QInputDialog::getText(this, QStringLiteral("手动写入"),
        QStringLiteral("为变量 %1 输入新值：").arg(tag.name), QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;

    QVariant value;
    QString error;
    if (!parseWriteValue(tag, input, value, error)) {
        QMessageBox::warning(this, QStringLiteral("写入失败"), error);
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("确认写入"),
            QStringLiteral("确认写入 %1 = %2？").arg(tag.name, value.toString())) != QMessageBox::Yes)
        return;

    WriteCommand cmd;
    cmd.tagId = tag.id;
    cmd.value = value;
    QMetaObject::invokeMethod(m_engine, [this, cmd]() { m_engine->enqueueWrite(cmd); },
        Qt::QueuedConnection);
    setStatus(QStringLiteral("已提交写入 %1 = %2").arg(tag.name, value.toString()));
}

void TagEditorWidget::onWriteCompleted(int tagId, bool success, const QString& error)
{
    if (success)
        setStatus(QStringLiteral("写入完成 tag %1").arg(tagId));
    else
        setStatus(QStringLiteral("写入失败 tag %1: %2").arg(tagId).arg(error));
}

void TagEditorWidget::setStatus(const QString& text)
{
    m_statusLabel->setText(text);
}
