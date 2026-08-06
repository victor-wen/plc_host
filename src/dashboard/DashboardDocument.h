#pragma once

#include <QJsonObject>
#include <QString>

// 看板页面模型 (docs/architecture/interfaces.md §9, Phase 2 DASH-01)
struct DashboardPage {
    int id = -1;              // -1 = 未保存（新页）
    QString name = "Page 1";
    int width = 1920;
    int height = 1080;
    QString background;       // 颜色（#RRGGBB）或图片资源路径；空 = 默认
    int sortOrder = 0;        // 页面排序
};

// 看板组件模型 (docs/architecture/interfaces.md §9, Phase 2 DASH-01)
// commonStyle 为表现层属性；config 含业务绑定，必须可独立反序列化，
// 损坏时由上层降级为 errorPlaceholder。
struct DashboardItem {
    int id = -1;
    int pageId = -1;
    QString itemType;         // text/rect/image/value/led/switch/progress/gauge/trend/button
    qreal x = 0, y = 0;
    qreal width = 100, height = 100;
    qreal zOrder = 0;
    QJsonObject commonStyle;  // fillColor, borderColor, borderWidth, font, radius...
    QJsonObject config;       // tagId, min, max, unit, action...（组件相关）
    int schemaVersion = 1;    // config JSON 结构版本
};
