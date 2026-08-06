#include "ui/PlcConfigWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

// 采集周期可选项（interfaces.md §4：200/500/1000/2000/5000ms）。
constexpr int kPollIntervals[] = {200, 500, 1000, 2000, 5000};

// 连接状态 → 显示文案（interfaces.md §8）。
QString stateText(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Disconnected:
        return QStringLiteral("未连接");
    case ConnectionState::Connecting:
        return QStringLiteral("连接中");
    case ConnectionState::Online:
        return QStringLiteral("在线");
    case ConnectionState::Degraded:
        return QStringLiteral("降级");
    case ConnectionState::Reconnecting:
        return QStringLiteral("重连中");
    }
    return QStringLiteral("未知");
}

// 连接状态 → 指示灯颜色（§8：Disconnected=灰, Connecting/Reconnecting=黄, Online=绿, Degraded=黄）。
QString stateColor(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Disconnected:
        return QStringLiteral("#888888");
    case ConnectionState::Connecting:
        return QStringLiteral("#f0c040");
    case ConnectionState::Online:
        return QStringLiteral("#2e9e44");
    case ConnectionState::Degraded:
        return QStringLiteral("#e6a23c");
    case ConnectionState::Reconnecting:
        return QStringLiteral("#e6a23c");
    }
    return QStringLiteral("#888888");
}

} // namespace

PlcConfigWidget::PlcConfigWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void PlcConfigWidget::buildUi()
{
    m_ipEdit = new QLineEdit(this);
    m_ipEdit->setPlaceholderText(QStringLiteral("192.168.1.100"));

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(0, 65535);

    m_unitIdSpin = new QSpinBox(this);
    m_unitIdSpin->setRange(1, 247);

    m_timeoutSpin = new QSpinBox(this);
    m_timeoutSpin->setRange(100, 10000);
    m_timeoutSpin->setSuffix(QStringLiteral(" ms"));

    m_retriesSpin = new QSpinBox(this);
    m_retriesSpin->setRange(0, 10);

    m_pollIntervalCombo = new QComboBox(this);
    for (int ms : kPollIntervals)
        m_pollIntervalCombo->addItem(QStringLiteral("%1 ms").arg(ms), ms);

    m_autoConnectCheck = new QCheckBox(QStringLiteral("启动时自动连接"), this);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("IP 地址"), m_ipEdit);
    form->addRow(QStringLiteral("端口"), m_portSpin);
    form->addRow(QStringLiteral("Unit ID"), m_unitIdSpin);
    form->addRow(QStringLiteral("超时"), m_timeoutSpin);
    form->addRow(QStringLiteral("重试次数"), m_retriesSpin);
    form->addRow(QStringLiteral("采集周期"), m_pollIntervalCombo);
    form->addRow(QString(), m_autoConnectCheck);

    auto* loadBtn = new QPushButton(QStringLiteral("加载"), this);
    auto* saveBtn = new QPushButton(QStringLiteral("保存"), this);
    auto* connectBtn = new QPushButton(QStringLiteral("连接"), this);
    auto* disconnectBtn = new QPushButton(QStringLiteral("断开"), this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(loadBtn);
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(connectBtn);
    btnRow->addWidget(disconnectBtn);
    btnRow->addStretch(1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setTextFormat(Qt::RichText);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addLayout(btnRow);
    root->addWidget(m_statusLabel);
    root->addStretch(1);

    connect(loadBtn, &QPushButton::clicked, this, [this]() { setConfig(m_config); });
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        m_config = config();
        emit saved(m_config);
    });
    connect(connectBtn, &QPushButton::clicked, this, [this]() { emit connectRequested(config()); });
    connect(disconnectBtn, &QPushButton::clicked, this, [this]() { emit disconnectRequested(); });

    setConfig(m_config);   // 初始化默认表单（host=192.168.1.100 等）
}

void PlcConfigWidget::setConfig(const PlcConfig& config)
{
    m_config = config;
    m_ipEdit->setText(config.host);
    m_portSpin->setValue(config.port);
    m_unitIdSpin->setValue(config.unitId);
    m_timeoutSpin->setValue(config.timeoutMs);
    m_retriesSpin->setValue(config.retries);
    const int idx = m_pollIntervalCombo->findData(config.pollIntervalMs);
    m_pollIntervalCombo->setCurrentIndex(idx >= 0 ? idx : 1);   // 缺省回退 500ms
    m_autoConnectCheck->setChecked(config.autoConnect);
}

PlcConfig PlcConfigWidget::config() const
{
    PlcConfig cfg;
    cfg.host = m_ipEdit->text().trimmed();
    cfg.port = m_portSpin->value();
    cfg.unitId = m_unitIdSpin->value();
    cfg.timeoutMs = m_timeoutSpin->value();
    cfg.retries = m_retriesSpin->value();
    cfg.pollIntervalMs = m_pollIntervalCombo->currentData().toInt();
    cfg.autoConnect = m_autoConnectCheck->isChecked();
    return cfg;
}

void PlcConfigWidget::setConnectionState(ConnectionState state)
{
    m_statusLabel->setText(QStringLiteral("<span style='color:%1; font-weight:bold;'>●</span> %2")
                               .arg(stateColor(state), stateText(state)));
}
