#pragma once

#include <QWidget>

#include "domain/PlcConfig.h"
#include "runtime/AcquisitionEngine.h"   // ConnectionState（§8 冻结枚举）

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

// PLC 连接配置表单（CORE-09）。
// 线程：UI 线程。不直接触碰通信对象；连接/断开经信号交由 MainWindow 转发。
// 表单字段：IP / Port / Unit ID / Timeout / Retries / PollInterval / AutoConnect。
class PlcConfigWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlcConfigWidget(QWidget* parent = nullptr);

    // 用配置填充表单（Load 按钮与 MainWindow 初始加载使用）。
    void setConfig(const PlcConfig& config);
    // 读取表单当前值（Save / Connect 使用）。
    PlcConfig config() const;

    // 由 MainWindow 转发 AcquisitionEngine::connectionStateChanged 驱动状态灯。
    void setConnectionState(ConnectionState state);

signals:
    void connectRequested(const PlcConfig& config);
    void disconnectRequested();
    void saved(const PlcConfig& config);

private:
    void buildUi();

    QLineEdit* m_ipEdit = nullptr;
    QSpinBox* m_portSpin = nullptr;
    QSpinBox* m_unitIdSpin = nullptr;
    QSpinBox* m_timeoutSpin = nullptr;
    QSpinBox* m_retriesSpin = nullptr;
    QComboBox* m_pollIntervalCombo = nullptr;
    QCheckBox* m_autoConnectCheck = nullptr;
    QLabel* m_statusLabel = nullptr;

    PlcConfig m_config;   // 最近一次 Load/Save 的配置（内存态，DB 持久化属后续任务）
};
