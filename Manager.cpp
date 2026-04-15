#include "Manager.h"
#include <iostream>

SystemManager::SystemManager(QObject* parent) : QObject(parent), is_system_ok(false) {
    // Проброс логов от всех подсистем наверх, в Manager
    connect(&canBus, &CanBusManager::logMessage, this, &SystemManager::logMessage);
    connect(&power, &PowerSupplyController::logMessage, this, &SystemManager::logMessage);
    connect(&cooling, &CoolingController::logMessage, this, &SystemManager::logMessage);
    
    emit logMessage("SystemManager: Объекты созданы.");
}

SystemManager::~SystemManager() {
    stopSystem();
}

void SystemManager::startSystem() {
    emit logMessage("SystemManager: System start in process...");
    if (!canBus.isOpen()) {
        if (!canBus.init()) {
            emit logMessage("SystemManager: CAN Start error! Stop.");
            return;
        }
    }
    
    power.setCanInterface(&canBus);
    
    cooling.setPumpState(true);
    cooling.setCoolerState(true);
    
    power.resetProtection();
    power.setPowerState(true);

    emit logMessage("Updating...");
    update();
    uint8_t status = power.getStatusFlags();
    if (status == 0x01) {
        emit logMessage("SystemManager: System started successfully.");
    }
    else {
        if (status & 0x02) {
            emit logMessage("SystemManager: Out protection 1!.");
        }
        if (status & 0x04) {
            emit logMessage("SystemManager: Out protection 2!.");
        }
        if (status & 0x08) {
            emit logMessage("SystemManager: Temperature protection!.");
        }
        if (status & 0x10) {
            emit logMessage("SystemManager: Invertor error!.");
        }
        if (status & 0x20) {
            emit logMessage("SystemManager: Phases error!.");
        }
        return;
    }

    is_system_ok = true;
}

void SystemManager::stopSystem() {
    if (!is_system_ok && !canBus.isOpen()) return;
    
    emit logMessage("SystemManager: Shutting down normally...");
    power.setPowerState(false);
    cooling.setPumpState(false);
    cooling.setCoolerState(false);
    
    canBus.close();
    is_system_ok = false;
    emit logMessage("SystemManager: System is off.");
}

void SystemManager::update() {
    // if (!is_system_ok || !canBus.isOpen()) return;
    if (!canBus.isOpen()) return;

    power.requestData(); 

    CAN_PACKET rcv;
    while (canBus.receivePacket(rcv)) {
        power.processRequestCanPacket(rcv);
        // Здесь можно будет добавлять обработку пакетов для других подсистем (например, охлаждения)
    }

    checkInterlocks(cooling.getFlowRate(), cooling.getTemperature(), power.getStatusFlags());
}

void SystemManager::checkInterlocks(float flow, float temp, uint8_t power_status) {
    bool alarm = false;
    
    if (flow < 2.0f) { 
        alarm = true;
        emit logMessage("ALARM: No water flow!");
    }
    if (temp > 75.0f) {
        alarm = true;
        emit logMessage("ALARM: Overheating!");
    }
    // Бит 1-2 (OutProt), Бит 3 (TempProt), Бит 4 (InvertProt), Бит 5 (PhaseProt)
    if (power_status & 0x2E) {
        alarm = true; 
        emit logMessage("ALARM: Power unit hardware defence!");
    }

    if (alarm) emergencyAllStop();
}

void SystemManager::setCurrent(float amperes) {
    if (is_system_ok) {
        power.setCurrent(amperes);
    } else {
        emit logMessage("Warning: Trying to set current while system is off.");
    }
}

void SystemManager::emergencyAllStop() {
    emit logMessage("SystemManager: EMERGENCY STOPPING!");
    is_system_ok = false;
    power.emergencyStop();
    cooling.emergencyStop();
}

float SystemManager::getTemp() { return cooling.getTemperature(); }
float SystemManager::getFlow() { return cooling.getFlowRate(); }
float SystemManager::getCurrent() { return power.getCurrent(); }
uint8_t SystemManager::getStatusFlags() { return power.getStatusFlags(); }
