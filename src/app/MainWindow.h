#pragma once

#include <QMainWindow>
#include <QVector>

#include "domain/PlcConfig.h"
#include "domain/Tag.h"
#include "runtime/AcquisitionEngine.h"   // ConnectionState（§8）

class QLabel;
class QListWidget;
class QStackedWidget;
class QThread;
class QtModbusClient;
class TagCache;
class PlcConfigWidget;
class TagEditorWidget;
class TagMonitorWidget;

// 主窗口（CORE-09）：左侧导航（PLC配置/Tag编辑/变量监视）+ 右侧工作区，
// 顶部工具栏显示连接状态。持有通信线程上的 AcquisitionEngine 并负责生命周期。
// 线程：UI 线程；对引擎的调用一律经 QMetaObject::invokeMethod(QueuedConnection)。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onNavRowChanged(int row);
    void onConnectRequested(const PlcConfig& config);
    void onDisconnectRequested();
    void onTagsChanged(const QVector<Tag>& tags);
    void onConnectionStateChanged(ConnectionState state);

private:
    void setupStatusToolbar();
    void setupNavigation();
    void setupCommThread();

    QListWidget* m_nav = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_statusLabel = nullptr;

    QThread* m_commThread = nullptr;
    QtModbusClient* m_client = nullptr;
    TagCache* m_cache = nullptr;
    AcquisitionEngine* m_engine = nullptr;

    PlcConfigWidget* m_plcConfig = nullptr;
    TagEditorWidget* m_tagEditor = nullptr;
    TagMonitorWidget* m_tagMonitor = nullptr;
};
