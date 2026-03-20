#include "CoolControl.h"
#include <iostream>

CoolingController::CoolingController() {}

void CoolingController::setPumpState(bool start) {
    std::cout << "[Cooling] Насос: " << (start ? "ВКЛ" : "ВЫКЛ") << std::endl;
}

void CoolingController::setCoolerState(bool start) {
    std::cout << "[Cooling] Кулер: " << (start ? "ВКЛ" : "ВЫКЛ") << std::endl;
}

float CoolingController::getTemperature() {
    return 24.5f; // Заглушка
}

float CoolingController::getFlowRate() {
    return 12.8f; // Заглушка
}

void CoolingController::emergencyStop() {
    std::cout << "[Cooling] !!! ОХЛАЖДЕНИЕ ОСТАНОВЛЕНО !!!" << std::endl;
}

void CoolingController::sendFrame(const CAN_Frame& frame) {
    // Низкоуровневая отправка
}