#include "CoolControl.h"
#include <iostream>

CoolingController::CoolingController() {}

void CoolingController::setPumpState(bool start) {
    emit logMessage(start ? "Cooling: Pump is on" : "Cooling: Pump is off.");
}

void CoolingController::setCoolerState(bool start) {
    emit logMessage(start ? "Cooling: Cooler is on" : "Cooling: Cooler is off.");
}

float CoolingController::getTemperature() {
    return 24.5f; // Заглушка
}

float CoolingController::getFlowRate() {
    return 12.8f; // Заглушка
}

void CoolingController::emergencyStop() {
    emit logMessage("CoolControl: EMERGENCY STOP !");
    setPumpState(false);
    setCoolerState(false);
}