set(DOMAIN_SOURCES ${CMAKE_SOURCE_DIR}/src/domain/Tag.cpp)
set(STORAGE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/storage/DatabaseMigrator.cpp
    ${CMAKE_SOURCE_DIR}/src/storage/BackupService.cpp
)
set(RECIPE_SOURCES ${CMAKE_SOURCE_DIR}/src/recipe/RecipeService.cpp)
set(LOGGING_SOURCES ${CMAKE_SOURCE_DIR}/src/logging/LogService.cpp)
set(HISTORY_SOURCES
    ${CMAKE_SOURCE_DIR}/src/history/HistoryService.cpp
    ${CMAKE_SOURCE_DIR}/src/history/CsvExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/history/TrendService.cpp
)
set(ALARM_SOURCES ${CMAKE_SOURCE_DIR}/src/alarm/AlarmEngine.cpp)
set(RUNTIME_SOURCES
    ${CMAKE_SOURCE_DIR}/src/runtime/AcquisitionEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/TagCache.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/WriteQueue.cpp
)
set(MODBUS_SOURCES
    ${CMAKE_SOURCE_DIR}/src/modbus/PollPlanner.cpp
    ${CMAKE_SOURCE_DIR}/src/modbus/QtModbusClient.cpp
    ${CMAKE_SOURCE_DIR}/src/modbus/ValueCodec.cpp
)
set(MODBUS_HEADERS
    ${CMAKE_SOURCE_DIR}/src/modbus/IModbusClient.h
    ${CMAKE_SOURCE_DIR}/src/modbus/QtModbusClient.h
)
set(DASHBOARD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardScene.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardView.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardController.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/runtime/ButtonActionExecutor.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/commands/AddItemCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/commands/RemoveItemCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/commands/MoveCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/commands/ResizeCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/commands/PropertyChangeCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/TextItem.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/RectItem.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/ImageItem.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/ValueItem.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/LedItem.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/SwitchItem.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/ProgressBarItem.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/GaugeItem.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/ValueInputItem.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/TrendItem.cpp
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/ButtonItem.cpp
)
set(DASHBOARD_HEADERS
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardDocument.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardRepository.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardBaseItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardScene.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardView.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardController.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/runtime/ButtonActionExecutor.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/commands/AddItemCommand.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/commands/RemoveItemCommand.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/commands/MoveCommand.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/commands/ResizeCommand.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/commands/PropertyChangeCommand.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/TextItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/RectItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/ImageItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/ValueItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/LedItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/SwitchItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/ProgressBarItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/GaugeItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/ValueInputItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/TrendItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/items/ButtonItem.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/runtime/ButtonAction.h
)
set(UI_SOURCES
    ${CMAKE_SOURCE_DIR}/src/ui/PlcConfigWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/TagEditorWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/TagMonitorWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/TrendWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/app/MainWindow.cpp
)
set(UI_HEADERS
    ${CMAKE_SOURCE_DIR}/src/ui/PlcConfigWidget.h
    ${CMAKE_SOURCE_DIR}/src/ui/TagEditorWidget.h
    ${CMAKE_SOURCE_DIR}/src/ui/TagMonitorWidget.h
    ${CMAKE_SOURCE_DIR}/src/ui/TrendWidget.h
    ${CMAKE_SOURCE_DIR}/src/app/MainWindow.h
)

function(add_qtest filepath)
    get_filename_component(target ${filepath} NAME_WE)
    add_executable(${target} ${filepath}.cpp ${DOMAIN_SOURCES} ${STORAGE_SOURCES}
        ${HISTORY_SOURCES}
        ${ALARM_SOURCES}
        ${RUNTIME_SOURCES} ${MODBUS_SOURCES} ${MODBUS_HEADERS}
        ${DASHBOARD_SOURCES} ${DASHBOARD_HEADERS}
        ${UI_SOURCES} ${UI_HEADERS}
        ${RECIPE_SOURCES} ${LOGGING_SOURCES})
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${target} PRIVATE
        Qt6::Test Qt6::Sql Qt6::SerialBus Qt6::Widgets Qt6::Charts)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_qtest(unit/tst_smoke)
add_qtest(unit/tst_DomainModels)
add_qtest(unit/tst_DatabaseMigrations)
add_qtest(unit/tst_ValueCodec)
add_qtest(unit/tst_WriteQueue)
add_qtest(unit/tst_TagCache)
add_qtest(unit/tst_PollPlanner)
add_qtest(unit/tst_DashboardRepository)
add_qtest(unit/tst_UndoCommands)
add_qtest(integration/tst_QtModbusClient)
add_qtest(integration/tst_AcquisitionEngine)
add_qtest(ui/tst_UiSmoke)
add_qtest(ui/tst_DashboardScene)
add_qtest(ui/tst_DashboardGeometry)
add_qtest(ui/tst_DashboardItems)
add_qtest(unit/tst_ButtonActionModel)
add_qtest(unit/tst_ButtonActionExecutor)
add_qtest(ui/tst_DashboardRuntime)
add_qtest(unit/tst_HistoryService)
add_qtest(unit/tst_CsvExporter)
add_qtest(ui/tst_TrendWidget)
add_qtest(unit/tst_AlarmEngine)
add_qtest(unit/tst_RecipeService)
add_qtest(unit/tst_LogService)
add_qtest(unit/tst_BackupService)
