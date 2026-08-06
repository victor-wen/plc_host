#include "history/CsvExporter.h"

#include <QFile>
#include <QHash>
#include <QMetaType>

namespace {

// CSV 字段转义：包含分隔符/引号/换行时用双引号包裹，内部引号翻倍。
QString csvField(const QString& raw)
{
    QString s = raw;
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"'))
        || s.contains(QLatin1Char('\n')) || s.contains(QLatin1Char('\r'))) {
        s.replace(QLatin1Char('"'), QLatin1String("\"\""));
        return QLatin1Char('"') + s + QLatin1Char('"');
    }
    return s;
}

// 数值保留合理小数位（最多 12 位有效数字，无多余尾零）。
QString formatValue(const QVariant& v)
{
    if (v.userType() == QMetaType::Double || v.userType() == QMetaType::Float)
        return QString::number(v.toDouble(), 'g', 12);
    if (v.userType() == QMetaType::Bool)
        return v.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    return v.toString();
}

QString registerTypeName(RegisterType rt)
{
    switch (rt) {
    case RegisterType::Coil:
        return QStringLiteral("Coil");
    case RegisterType::DiscreteInput:
        return QStringLiteral("DiscreteInput");
    case RegisterType::InputRegister:
        return QStringLiteral("InputRegister");
    case RegisterType::HoldingRegister:
        return QStringLiteral("HoldingRegister");
    }
    return QStringLiteral("Unknown");
}

QString dataTypeName(DataType dt)
{
    switch (dt) {
    case DataType::Bool:
        return QStringLiteral("Bool");
    case DataType::Int16:
        return QStringLiteral("Int16");
    case DataType::UInt16:
        return QStringLiteral("UInt16");
    case DataType::Int32:
        return QStringLiteral("Int32");
    case DataType::UInt32:
        return QStringLiteral("UInt32");
    case DataType::Float32:
        return QStringLiteral("Float32");
    case DataType::BitField:
        return QStringLiteral("BitField");
    }
    return QStringLiteral("Unknown");
}

QString byteOrderName(ByteOrder bo)
{
    switch (bo) {
    case ByteOrder::ABCD:
        return QStringLiteral("ABCD");
    case ByteOrder::DCBA:
        return QStringLiteral("DCBA");
    case ByteOrder::BADC:
        return QStringLiteral("BADC");
    case ByteOrder::CDAB:
        return QStringLiteral("CDAB");
    }
    return QStringLiteral("Unknown");
}

QString qualityName(Quality quality)
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

// 写 UTF-8 BOM + 内容。返回是否完整写入。
bool writeFile(const QString& filePath, const QString& content)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    QByteArray bytes = content.toUtf8();
    bytes.prepend(QByteArray::fromHex("EFBBBF"));
    const qint64 written = file.write(bytes);
    return written == bytes.size();
}

} // namespace

bool CsvExporter::exportHistory(const QString& filePath,
                                const QVector<TagValue>& data,
                                const QVector<Tag>& tags)
{
    QHash<int, const Tag*> tagById;
    tagById.reserve(tags.size());
    for (const Tag& t : tags)
        tagById.insert(t.id, &t);

    QString csv;
    csv.append(QStringLiteral("Time,TagName,Value,Quality,Unit\n"));
    for (const TagValue& tv : data) {
        QString tagName;
        QString unit;
        const Tag* tag = tagById.value(tv.tagId, nullptr);
        if (tag != nullptr) {
            tagName = tag->name;
            unit = tag->unit;
        }
        const QString time = tv.timestamp.isValid()
            ? tv.timestamp.toString(Qt::ISODateWithMs)
            : QString();
        csv.append(csvField(time));
        csv.append(QLatin1Char(','));
        csv.append(csvField(tagName));
        csv.append(QLatin1Char(','));
        csv.append(csvField(formatValue(tv.value)));
        csv.append(QLatin1Char(','));
        csv.append(csvField(qualityName(tv.quality)));
        csv.append(QLatin1Char(','));
        csv.append(csvField(unit));
        csv.append(QLatin1Char('\n'));
    }
    return writeFile(filePath, csv);
}

bool CsvExporter::exportTags(const QString& filePath, const QVector<Tag>& tags)
{
    QString csv;
    csv.append(QStringLiteral(
        "Name,RegisterType,Address,DataType,ByteOrder,Scale,Offset,Unit\n"));
    for (const Tag& t : tags) {
        csv.append(csvField(t.name));
        csv.append(QLatin1Char(','));
        csv.append(csvField(registerTypeName(t.registerType)));
        csv.append(QLatin1Char(','));
        csv.append(QString::number(t.address));
        csv.append(QLatin1Char(','));
        csv.append(csvField(dataTypeName(t.dataType)));
        csv.append(QLatin1Char(','));
        csv.append(csvField(byteOrderName(t.byteOrder)));
        csv.append(QLatin1Char(','));
        csv.append(QString::number(t.scale, 'g', 12));
        csv.append(QLatin1Char(','));
        csv.append(QString::number(t.offset, 'g', 12));
        csv.append(QLatin1Char(','));
        csv.append(csvField(t.unit));
        csv.append(QLatin1Char('\n'));
    }
    return writeFile(filePath, csv);
}
