#include "PowerControl.h"
#include <iostream>

PowerSupplyController::PowerSupplyController() : card_handle(0) {}

bool PowerSupplyController::init() {
    std::cout << "[Power] Initialization..." << std::endl;
    return true; 
}

void PowerSupplyController::setCurrent(float amperes) {
    std::cout << "[CAN] Команда: Установить ток " << amperes << " A" << std::endl;
    // Здесь будет упаковка в CAN_Frame и вызов sendFrame
}

float PowerSupplyController::getCurrent() {
    return 42.0f; // Заглушка
}

void PowerSupplyController::emergencyStop() {
    std::cout << "[Power] !!! ПИТАНИЕ ОТКЛЮЧЕНО !!!" << std::endl;
}

void PowerSupplyController::sendFrame(const CAN_Frame& frame) {
    // Низкоуровневая отправка через ADLINK SDK
}