#include "CoolControl.h"
#include <iostream>

CoolingController::CoolingController() {}

void CoolingController::setPumpState(bool start) {
    std::cout << "[Cooling] Pump is: " << (start ? "ON" : "OFF") << std::endl;
}

void CoolingController::setCoolerState(bool start) {
    std::cout << "[Cooling] Cooling is: " << (start ? "ON" : "OFF") << std::endl;
}

float CoolingController::getTemperature() {
    return 24.5f; // Заглушка
}

float CoolingController::getFlowRate() {
    return 12.8f; // Заглушка
}

void CoolingController::emergencyStop() {
    std::cout << "[Cooling] !!! COOLING IS OFF !!!" << std::endl;
}

void CoolingController::sendFrame(const CAN_Frame& frame) {
    // Низкоуровневая отправка
}