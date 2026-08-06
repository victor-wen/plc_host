set(SOURCES
    app/main.cpp
    app/MainWindow.cpp
    app/AppContext.cpp
    domain/Tag.cpp
    modbus/PollPlanner.cpp
    modbus/QtModbusClient.cpp
    modbus/ValueCodec.cpp
    runtime/AcquisitionEngine.cpp
    runtime/TagCache.cpp
    runtime/WriteQueue.cpp
    storage/DatabaseMigrator.cpp
    history/HistoryService.cpp
    dashboard/DashboardRepository.cpp
    dashboard/DashboardScene.cpp
    dashboard/DashboardView.cpp
    dashboard/DashboardController.cpp
    dashboard/runtime/ButtonActionExecutor.cpp
    dashboard/commands/AddItemCommand.cpp
    dashboard/commands/RemoveItemCommand.cpp
    dashboard/commands/MoveCommand.cpp
    dashboard/commands/ResizeCommand.cpp
    dashboard/commands/PropertyChangeCommand.cpp
    dashboard/items/TextItem.cpp
    dashboard/items/RectItem.cpp
    dashboard/items/ImageItem.cpp
    dashboard/items/ValueItem.cpp
    dashboard/items/LedItem.cpp
    dashboard/items/SwitchItem.cpp
    dashboard/items/ProgressBarItem.cpp
    dashboard/items/GaugeItem.cpp
    dashboard/items/ValueInputItem.cpp
    dashboard/items/TrendItem.cpp
    dashboard/items/ButtonItem.cpp
    ui/PlcConfigWidget.cpp
    ui/TagEditorWidget.cpp
    ui/TagMonitorWidget.cpp
)

set(HEADERS
    app/MainWindow.h
    app/AppContext.h
    domain/PlcConfig.h
    domain/Tag.h
    domain/TagValue.h
    modbus/IModbusClient.h
    modbus/PollPlanner.h
    modbus/QtModbusClient.h
    modbus/ValueCodec.h
    runtime/AcquisitionEngine.h
    runtime/TagCache.h
    runtime/WriteQueue.h
    storage/DatabaseMigrator.h
    history/HistoryService.h
    dashboard/DashboardDocument.h
    dashboard/DashboardRepository.h
    dashboard/DashboardBaseItem.h
    dashboard/DashboardScene.h
    dashboard/DashboardView.h
    dashboard/DashboardController.h
    dashboard/runtime/ButtonActionExecutor.h
    dashboard/commands/AddItemCommand.h
    dashboard/commands/RemoveItemCommand.h
    dashboard/commands/MoveCommand.h
    dashboard/commands/ResizeCommand.h
    dashboard/commands/PropertyChangeCommand.h
    dashboard/items/TextItem.h
    dashboard/items/RectItem.h
    dashboard/items/ImageItem.h
    dashboard/items/ValueItem.h
    dashboard/items/LedItem.h
    dashboard/items/SwitchItem.h
    dashboard/items/ProgressBarItem.h
    dashboard/items/GaugeItem.h
    dashboard/items/ValueInputItem.h
    dashboard/items/TrendItem.h
    dashboard/items/ButtonItem.h
    dashboard/runtime/ButtonAction.h
    ui/PlcConfigWidget.h
    ui/TagEditorWidget.h
    ui/TagMonitorWidget.h
)

qt_add_executable(plc_host ${SOURCES} ${HEADERS})

target_include_directories(plc_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(plc_host PRIVATE
    Qt6::Widgets
    Qt6::SerialBus
    Qt6::Sql
    Qt6::Charts
)

set_target_properties(plc_host PROPERTIES WIN32_EXECUTABLE ON)
