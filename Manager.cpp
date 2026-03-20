#include "Manager.h"
#include <iostream>

SystemManager::SystemManager() : is_system_ok(true) {
    power.init();
}

void SystemManager::update() {
    float flow = cooling.getFlowRate();
    float temp = cooling.getTemperature();

    // Блокировка 1: Утечка (Поток < 2.0)
    if (flow < 2.0f && is_system_ok) {
        std::cerr << "[SYSTEM] КРИТИЧЕСКИЙ ОТКАЗ: НЕТ ПОТОКА!" << std::endl;
        emergencyAllStop();
    }

    // Блокировка 2: Перегрев (Температура > 65.0)
    if (temp > 65.0f) {
        power.emergencyStop();
    }
}

void SystemManager::startSystem() {
    cooling.setPumpState(true);
    cooling.setCoolerState(true);
    power.init();
}

void SystemManager::setCurrent(float amperes) {
    power.setCurrent(amperes);
}

void SystemManager::emergencyAllStop() {
    is_system_ok = false;
    power.emergencyStop();
    cooling.emergencyStop();
}