#pragma once

#include <QString>

struct PlcConfig {
    int id = 1;
    QString name;
    QString host = "192.168.1.100";
    int port = 502;
    int unitId = 1;
    int timeoutMs = 1000;
    int retries = 2;
    int pollIntervalMs = 500;
    bool autoConnect = true;
};
