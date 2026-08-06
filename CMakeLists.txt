cmake_minimum_required(VERSION 3.24)
project(plc_host VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(BUILD_TESTING "Enable tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialBus Sql Test Charts)

include(cmake/Warnings.cmake)
qt_standard_project_setup()

add_subdirectory(src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
