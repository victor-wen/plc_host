#include "app/MainWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QThread>
#include <QToolBar>
#include <QVBoxLayout>

#include "modbus/QtModbusClient.h"
#include "runtime/TagCache.h"
#include "ui/PlcConfigWidget.h"
#include "ui/TagEditorWidget.h"
#include "ui/TagMonitorWidget.h"

namespace {

QString statusText(ConnectionState state)
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

QString statusColor(ConnectionState state)
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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("PLC Host v0.1.0"));
    resize(1280, 800);

    setupStatusToolbar();
    setupNavigation();

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_nav);
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);

    setupCommThread();
}

MainWindow::~MainWindow()
{
    // 先让通信线程执行 stop()（手动断开语义，不自动重连），再退出事件循环。
    if (m_engine != nullptr)
        QMetaObject::invokeMethod(m_engine, "stop", Qt::BlockingQueuedConnection);
    if (m_commThread != nullptr) {
        m_commThread->quit();
        m_commThread->wait();
    }
    delete m_engine;
    delete m_client;
    delete m_cache;
}

void MainWindow::setupStatusToolbar()
{
    auto* toolbar = addToolBar(QStringLiteral("状态"));
    toolbar->setObjectName(QStringLiteral("statusToolBar"));
    toolbar->setMovable(false);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setTextFormat(Qt::RichText);
    toolbar->addWidget(m_statusLabel);
}

void MainWindow::setupNavigation()
{
    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("mainNav"));
    m_nav->addItem(QStringLiteral("PLC 配置"));
    m_nav->addItem(QStringLiteral("Tag 编辑"));
    m_nav->addItem(QStringLiteral("变量监视"));
    m_nav->setFixedWidth(160);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("mainStack"));

    m_plcConfig = new PlcConfigWidget(m_stack);
    m_tagEditor = new TagEditorWidget(m_stack);
    m_tagMonitor = new TagMonitorWidget(m_stack);

    m_stack->addWidget(m_plcConfig);
    m_stack->addWidget(m_tagEditor);
    m_stack->addWidget(m_tagMonitor);

    connect(m_nav, &QListWidget::currentRowChanged, this, &MainWindow::onNavRowChanged);
    m_nav->setCurrentRow(0);
}

void MainWindow::setupCommThread()
{
    m_cache = new TagCache(1024);
    m_client = new QtModbusClient;
    m_engine = new AcquisitionEngine(m_client, m_cache);

    m_commThread = new QThread(this);
    m_client->moveToThread(m_commThread);
    m_engine->moveToThread(m_commThread);
    m_commThread->start();

    // 引擎（通信线程）→ UI 线程：连接时自动使用 QueuedConnection。
    connect(m_engine, &AcquisitionEngine::connectionStateChanged,
        this, &MainWindow::onConnectionStateChanged);
    connect(m_engine, &AcquisitionEngine::connectionStateChanged,
        m_plcConfig, &PlcConfigWidget::setConnectionState);
    m_tagMonitor->setEngine(m_engine);
    m_tagEditor->setEngine(m_engine);

    // 页面间转发。
    connect(m_tagEditor, &TagEditorWidget::tagsChanged,
        this, &MainWindow::onTagsChanged);
    connect(m_plcConfig, &PlcConfigWidget::connectRequested,
        this, &MainWindow::onConnectRequested);
    connect(m_plcConfig, &PlcConfigWidget::disconnectRequested,
        this, &MainWindow::onDisconnectRequested);

    m_plcConfig->setConfig(PlcConfig{});   // 表单默认值（192.168.1.100:502）
    onConnectionStateChanged(ConnectionState::Disconnected);
}

void MainWindow::onNavRowChanged(int row)
{
    if (row >= 0 && row < m_stack->count())
        m_stack->setCurrentIndex(row);
}

void MainWindow::onConnectRequested(const PlcConfig& config)
{
    // 连接参数先下发到客户端（超时/重试），再启动引擎（均在其线程内执行）。
    QMetaObject::invokeMethod(m_client, [this, config]() {
        m_client->setTimeout(config.timeoutMs);
        m_client->setNumberOfRetries(config.retries);
    }, Qt::QueuedConnection);

    QMetaObject::invokeMethod(m_engine, [this, config]() {
        m_engine->start(config.host, config.port, config.unitId);
    }, Qt::QueuedConnection);
}

void MainWindow::onDisconnectRequested()
{
    QMetaObject::invokeMethod(m_engine, [this]() { m_engine->stop(); }, Qt::QueuedConnection);
}

void MainWindow::onTagsChanged(const QVector<Tag>& tags)
{
    QMetaObject::invokeMethod(m_engine, [this, tags]() { m_engine->setTags(tags); },
        Qt::QueuedConnection);
    m_tagMonitor->setTags(tags);
}

void MainWindow::onConnectionStateChanged(ConnectionState state)
{
    m_statusLabel->setText(QStringLiteral("<span style='color:%1; font-weight:bold;'>●</span> %2")
                               .arg(statusColor(state), statusText(state)));
}
