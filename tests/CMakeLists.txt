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

function(add_qtest filepath)
    get_filename_component(target ${filepath} NAME_WE)
    add_executable(${target} ${filepath}.cpp ${DOMAIN_SOURCES} ${STORAGE_SOURCES}
        ${RUNTIME_SOURCES} ${MODBUS_SOURCES} ${MODBUS_HEADERS})
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${target} PRIVATE Qt6::Test Qt6::Sql Qt6::SerialBus)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_qtest(unit/tst_smoke)
add_qtest(unit/tst_DomainModels)
add_qtest(unit/tst_DatabaseMigrations)
add_qtest(unit/tst_ValueCodec)
add_qtest(unit/tst_WriteQueue)
add_qtest(unit/tst_TagCache)
add_qtest(unit/tst_PollPlanner)
add_qtest(integration/tst_QtModbusClient)
add_qtest(integration/tst_AcquisitionEngine)
