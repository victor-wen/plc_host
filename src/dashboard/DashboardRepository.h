#pragma once

#include <QObject>
#include <QVector>

#include "DashboardDocument.h"

// 看板持久化仓储 (docs/architecture/interfaces.md §10, Phase 2 DASH-01)
// 线程: 数据库线程。持有命名连接，连接须已 open；跨线程调用必须经
// 信号/槽或 QMetaObject::invokeMethod 转到数据库线程执行。
// 所有方法同步阻塞，失败返回 false 或空容器。
class DashboardRepository : public QObject {
    Q_OBJECT
public:
    explicit DashboardRepository(const QString& connectionName, QObject* parent = nullptr);

    QVector<DashboardPage> loadPages();
    bool savePage(DashboardPage& page);       // id=-1 时 INSERT 并回填 id，否则 UPDATE
    bool deletePage(int pageId);              // 级联删除该页 items（DB 外键 CASCADE）
    QVector<DashboardItem> loadItems(int pageId);
    bool saveItems(int pageId, const QVector<DashboardItem>& items); // 事务内 DELETE+INSERT

private:
    QString m_connectionName;
};
