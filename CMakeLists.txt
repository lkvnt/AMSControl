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

set(PROJECT_SOURCES
    main.cpp
    
    src/core/Manager.cpp
    src/core/SettingsManager.cpp

    src/hardware/CanBusManager.cpp
    src/hardware/PowerControl.cpp
    src/hardware/CoolControl.cpp
    src/hardware/SensorControl.cpp
    src/hardware/HardwareSimulator.cpp

    src/gui/QtUI.cpp
    src/gui/DataViewerWindow.cpp
    src/gui/SettingsDialog.cpp

    src/logging/LoggerTXT.cpp
    src/logging/LoggerCSV.cpp
    src/logging/LogWorker.cpp
)

add_executable(${PROJECT_NAME} WIN32 ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hardware
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logging
)

target_link_libraries(AMSControl PRIVATE Qt6::Widgets Qt6::Charts 7841_x64)
target_compile_options(AMSControl PRIVATE $<$<CONFIG:Release>:/O2>)

if(WIN32)
    qt_finalize_executable(AMSControl)
endif()
