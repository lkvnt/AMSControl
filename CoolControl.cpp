#include "CoolControl.h"
#include <iostream>

CoolingController::CoolingController() {}

void CoolingController::setPumpState(bool start) {
    emit logMessage(start ? "Cooling: Pump is on" : "Cooling: Pump is off.");
}

void CoolingController::setCoolerState(bool start) {
    emit logMessage(start ? "Cooling: Cooler is on" : "Cooling: Cooler is off.");
}

void CoolingController::emergencyStop() {
    emit logMessage("CoolControl: EMERGENCY STOP !");
    setPumpState(false);
    setCoolerState(false);
}