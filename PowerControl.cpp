#include "PowerControl.h"

PowerSupplyController::PowerSupplyController(uint8_t deviceId, QObject* parent) 
    : QObject(parent), can(nullptr), dev_id(deviceId), current_actual(0.0f), status_flags(0) {}

void PowerSupplyController::setCanInterface(CanBusManager* can_interface) {
    can = can_interface;
    emit logMessage("PowerControl: CAN interface connected.");
}

// Не оч понял зачем это надо
uint32_t PowerSupplyController::getTargetId() const {
    return (6 << 8) | (dev_id << 2); // Адресная посылка (Приоритет 6)
}

void PowerSupplyController::setPowerState(bool turnOn) {
    // F9 - Выходной регистр: бит 0 = ВКЛ (Контакты 30,12), бит 1 = ВЫКЛ (Контакты 31,13)
    if (!can) return;
    uint8_t state = turnOn ? 0x01 : 0x02; 
    can->sendCommand(getTargetId(), 0xF9, {state});
    emit logMessage(turnOn ? "PowerControl: Command power on." : "PowerControl: Command power off.");
}

void PowerSupplyController::setCurrent(float amperes) {
    // 80 - Запись ЦАП. 24-битный формат. FFFFF8 = 10В. (Предполагаем 8В = 300А на CLM-1000)
    if (!can) return;
    float voltage = (amperes / 300.0f) * 8.0f;
    if (voltage > 8.0f) voltage = 8.0f;
    if (voltage < -8.0f) voltage = -8.0f;
    
    uint32_t dac_code = static_cast<uint32_t>((voltage / 10.0f) * 0xFFFFF8);
    
    can->sendCommand(getTargetId(), 0x80, {
        static_cast<uint8_t>((dac_code >> 16) & 0xFF),
        static_cast<uint8_t>((dac_code >> 8) & 0xFF),
        static_cast<uint8_t>(dac_code & 0xFF),
        0x00, 0x00, 0x00 
    });
    emit logMessage(QString("PowerControl: Setting current %1 А").arg(amperes));
}

void PowerSupplyController::requestData() {
    if (!can) return;
    can->sendCommand(getTargetId(), 0x02, {0x00, 0x00, 0x20}); // Запрос АЦП
    can->sendCommand(getTargetId(), 0xF8, {});                 // Запрос статуса
}

void PowerSupplyController::emergencyStop() {
    setPowerState(false);
    setCurrent(0.0f);
    emit logMessage("PowerControl: EMERGENCY STOP!");
}

void PowerSupplyController::processCanPacket(const CAN_PACKET& rcv) {
    uint32_t reply_id = (7 << 8) | (dev_id << 2);
    if (rcv.CAN_ID != reply_id) return;

    if (rcv.data[0] == 0x02 && rcv.len >= 5) {
        uint32_t adc_code = (rcv.data[4] << 16) | (rcv.data[3] << 8) | rcv.data[2];
        float voltage = (adc_code / static_cast<float>(0x3FFFFF)) * 10.0f;
        current_actual = (voltage / 8.0f) * 300.0f; 
    } 
    else if (rcv.data[0] == 0xF8 && rcv.len >= 3) {
        status_flags = rcv.data[2]; 
    }
}
