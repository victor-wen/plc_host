#pragma once

#include <QString>

// 数据库备份/恢复服务（MON-07）。
// 通过 VACUUM INTO 获取 SQLite 一致快照（含 WAL 中未落盘的已提交数据）；
// restore 遵循“先备份当前 DB → 验证目标版本 → 替换”流程，并重建既有连接。
class BackupService {
public:
    // dbPath：当前数据库文件路径；connectionName：应用既有连接名（restore 时关闭/重开）。
    BackupService(const QString& dbPath, const QString& connectionName);

    // 将当前数据库备份为 filePath（文件已存在时覆盖）。
    bool backup(const QString& filePath);

    // 恢复：备份当前 DB 到 .pre-restore.<时间戳>.db → verifySchema 通过 → 替换 → 重开连接。
    bool restore(const QString& filePath);

    // 验证 filePath 是本应用 schema 且版本兼容（schema_migrations ≥ 1 + 核心表齐全）。
    bool verifySchema(const QString& filePath);

private:
    QString m_dbPath;
    QString m_connectionName;
    int m_connCounter = 0;
};
