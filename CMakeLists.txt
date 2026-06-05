cmake_minimum_required(VERSION 3.16)
project(AMSControl VERSION 1.0 LANGUAGES CXX)

# Автоматическая обработка Qt: MOC (для сигналов/слотов), RCC и UIC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

# 1. Ищем Qt 6
find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

# 2. Подключаем сторонние библиотеки (ADLINK)
# Укажите путь к папке с заголовками и .lib файлом ADLINK
include_directories("C:/Program Files/ADLINK/PCI-7841/Include") 
link_directories("C:/Program Files/ADLINK/PCI-7841/Include")

# 3. Список ваших исходников
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
)

# 4. Создаем исполняемый файл
add_executable(AMSControl ${SOURCES})

# 5. Линковка библиотек
target_link_libraries(AMSControl PRIVATE Qt6::Widgets Qt6::Charts 7841_x64)

# Копирование необходимых DLL (для Windows) при сборке
if(WIN32)
    qt_finalize_executable(AMSControl)
endif()
