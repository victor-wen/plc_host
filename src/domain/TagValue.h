#pragma once

#include <QVariant>
#include <QDateTime>
#include <QString>

enum class Quality {
    Good,
    Stale,
    Bad,
    Disconnected
};

struct TagValue {
    int tagId = -1;
    QVariant value;
    QVariant rawValue;
    Quality quality = Quality::Disconnected;
    QDateTime timestamp;
    QString error;
};
