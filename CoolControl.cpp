#include "CoolControl.h"
#include "SettingsManager.h"
#include <iostream>

CoolingController::CoolingController() {}

void CoolingController::setPumpState(bool start) {
    pumpState = start ? true : false;
    emit logMessage(start ? "Cooling: Pump is on" : "Cooling: Pump is off.");
}

void CoolingController::setCoolerState(bool start) {
    coolState = start ? true : false;
    emit logMessage(start ? "Cooling: Cooler is on" : "Cooling: Cooler is off.");
}

float CoolingController::getTemperature() const {
    return SettingsManager::instance().get("cool_mockTemp").toFloat();
}

float CoolingController::getFlowRate() const {
    return SettingsManager::instance().get("cool_mockFlow").toFloat();
}