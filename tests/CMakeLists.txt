set(DOMAIN_SOURCES ${CMAKE_SOURCE_DIR}/src/domain/Tag.cpp)
set(STORAGE_SOURCES ${CMAKE_SOURCE_DIR}/src/storage/DatabaseMigrator.cpp)
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
)
set(DASHBOARD_HEADERS
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardDocument.h
    ${CMAKE_SOURCE_DIR}/src/dashboard/DashboardRepository.h
)
set(UI_SOURCES
    ${CMAKE_SOURCE_DIR}/src/ui/PlcConfigWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/TagEditorWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/TagMonitorWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/app/MainWindow.cpp
)
set(UI_HEADERS
    ${CMAKE_SOURCE_DIR}/src/ui/PlcConfigWidget.h
    ${CMAKE_SOURCE_DIR}/src/ui/TagEditorWidget.h
    ${CMAKE_SOURCE_DIR}/src/ui/TagMonitorWidget.h
    ${CMAKE_SOURCE_DIR}/src/app/MainWindow.h
)

function(add_qtest filepath)
    get_filename_component(target ${filepath} NAME_WE)
    add_executable(${target} ${filepath}.cpp ${DOMAIN_SOURCES} ${STORAGE_SOURCES}
        ${RUNTIME_SOURCES} ${MODBUS_SOURCES} ${MODBUS_HEADERS}
        ${DASHBOARD_SOURCES} ${DASHBOARD_HEADERS}
        ${UI_SOURCES} ${UI_HEADERS})
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${target} PRIVATE Qt6::Test Qt6::Sql Qt6::SerialBus Qt6::Widgets)
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
add_qtest(integration/tst_QtModbusClient)
add_qtest(integration/tst_AcquisitionEngine)
add_qtest(ui/tst_UiSmoke)
