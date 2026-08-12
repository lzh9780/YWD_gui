cmake_minimum_required(VERSION 3.10)
project(YWD_Motor_Control VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Core Charts)

# ZLG driver library (adjust path according to your installation)
# The library is typically libusbcanfd.so
set(ZLG_LIB_DIR "" CACHE PATH "Path to ZLG CAN library directory")
set(ZLG_INCLUDE_DIR "" CACHE PATH "Path to ZLG CAN header directory")

if(ZLG_LIB_DIR)
    link_directories(${ZLG_LIB_DIR})
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${ZLG_INCLUDE_DIR}
)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/canfd_device.cpp
    src/ywd_protocol.cpp
    src/YwdDiagramWidget.cpp
    src/YwdPlotPanel.cpp
)

set(HEADERS
    include/mainwindow.h
    include/canfd_device.h
    include/ywd_protocol.h
    include/zcan.h
    include/YwdDiagramWidget.hpp
    include/YwdPlotPanel.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Widgets
    Qt5::Core
    Qt5::Charts
)

# Link ZLG CAN library
if(ZLG_LIB_DIR)
    target_link_libraries(${PROJECT_NAME} PRIVATE usbcanfd)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE usbcanfd)
endif()
