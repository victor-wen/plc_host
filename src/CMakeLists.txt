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
    dashboard/DashboardRepository.cpp
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
    dashboard/DashboardDocument.h
    dashboard/DashboardRepository.h
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
