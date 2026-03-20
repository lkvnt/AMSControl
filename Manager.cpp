#include "Manager.h"
#include <iostream>

SystemManager::SystemManager() : is_system_ok(false) {std::cout << "Manager here\n";}

void SystemManager::startSystem() {
    cooling.setPumpState(true);
    cooling.setCoolerState(true);
    bool powerInitResult = power.init();
    if (powerInitResult) {
        is_system_ok = true;
        power.setPowerState(true);
    }
}

void SystemManager::stopSystem() {
    power.setPowerState(false);
    cooling.setPumpState(false);
    cooling.setCoolerState(false);
    is_system_ok = false;
}

void SystemManager::update() {
    if (!is_system_ok) return;

    power.processCanMessages(); 

    float flow = cooling.getFlowRate();
    float temp = cooling.getTemperature();
    uint8_t p_status = power.getStatusFlags();

    checkInterlocks(flow, temp, p_status);
}

void SystemManager::checkInterlocks(float flow, float temp, uint8_t power_status) {
    bool alarm = false;
    
    if (flow < 2.0f) alarm = true;
    if (temp > 65.0f) alarm = true;
    
    // Бит 1 (OutProt), Бит 2 (TempProt), Бит 3 (InvertProt), Бит 4 (PhaseProt)
    if (power_status & 0x1E) alarm = true; 

    if (alarm) emergencyAllStop();
}

void SystemManager::setCurrent(float amperes) {
    if (is_system_ok) {
        power.setCurrent(amperes);
    }
}

void SystemManager::emergencyAllStop() {
    is_system_ok = false;
    power.emergencyStop();
    cooling.emergencyStop();
}

float SystemManager::getTemp() { return cooling.getTemperature(); }
float SystemManager::getFlow() { return cooling.getFlowRate(); }
float SystemManager::getCurrent() { return power.getCurrent(); }
uint8_t SystemManager::getStatusFlags() { return power.getStatusFlags(); }
