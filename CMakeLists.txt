cmake_minimum_required(VERSION 3.16)
project(AMSControl VERSION 1.0 LANGUAGES CXX)

# Автоматическая обработка Qt: MOC (для сигналов/слотов), RCC и UIC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Ищем Qt 6
find_package(Qt6 REQUIRED COMPONENTS Widgets)

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
    PowerControl.cpp
    PowerControl.h
    CoolControl.cpp
    CoolControl.h
)

# 4. Создаем исполняемый файл
add_executable(AMSControl ${SOURCES})

# 5. Линковка библиотек
# Qt6::Widgets — это база для интерфейса
# Pci7841 — имя библиотеки ADLINK (замените на точное имя вашего .lib/.a файла)
target_link_libraries(AMSControl PRIVATE Qt6::Widgets 7841_x64)