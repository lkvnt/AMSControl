cmake_minimum_required(VERSION 3.16)
project(AMSControl VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

include_directories("C:/Program Files/ADLINK/PCI-7841/Include") 
link_directories("C:/Program Files/ADLINK/PCI-7841/Include")

set(SOURCES
    main.cpp
    QtUI.cpp
    QtUI.h
    Manager.cpp
    Manager.h
    CanBusManager.h
    CanBusManager.cpp
    PowerControl.cpp
    PowerControl.h
    CoolControl.cpp
    CoolControl.h
    SensorControl.cpp
    SensorControl.h
    DataViewerWindow.h
    DataViewerWindow.cpp
    SettingsDialog.h
    SettingsDialog.cpp
    SettingsManager.h
    SettingsManager.cpp
    LogWorker.h
    LogWorker.cpp
    Logger.h
    LoggerTXT.h
    LoggerTXT.cpp
    LoggerCSV.h
    LoggerCSV.cpp
    Theme.h
    HardwareSimulator.h
    HardwareSimulator.cpp
)

add_executable(AMSControl WIN32 ${SOURCES})

target_link_libraries(AMSControl PRIVATE Qt6::Widgets Qt6::Charts 7841_x64)
target_compile_options(AMSControl PRIVATE $<$<CONFIG:Release>:/O2>)

if(WIN32)
    qt_finalize_executable(AMSControl)
endif()
