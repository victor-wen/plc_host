#pragma once

#include <QString>
#include <QVector>

#include "domain/Tag.h"
#include "domain/TagValue.h"

// CSV 导出服务 (MON-02)：历史样本与 Tag 定义导出为 CSV 文件。
// 统一输出 UTF-8 BOM (EF BB BF) 头，便于 Excel/WPS 正确识别中文编码。
// 纯静态工具类，不持有状态，可在任意线程调用。
class CsvExporter {
public:
    // 导出历史样本。列：Time,TagName,Value,Quality,Unit。
    // tags 用于将 tagId 解析为 Tag 名称与单位；未匹配的 tagId 输出空名称/单位。
    // 返回是否写入成功（含内容为空但表头已写入的情况）。
    static bool exportHistory(const QString& filePath,
                              const QVector<TagValue>& data,
                              const QVector<Tag>& tags);

    // 导出 Tag 定义。列：Name,RegisterType,Address,DataType,ByteOrder,Scale,Offset,Unit。
    static bool exportTags(const QString& filePath, const QVector<Tag>& tags);
};
