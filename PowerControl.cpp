#include "PowerControl.h"

PowerSupplyController::PowerSupplyController(uint8_t deviceId, QObject* parent) 
    : QObject(parent), can(nullptr), dev_id(deviceId), current_actual(0.0f), status_flags(0) {}

void PowerSupplyController::setCanInterface(CanBusManager* can_interface) {
    can = can_interface;
    emit logMessage("Power Control: CAN interface connected.");
}

uint32_t PowerSupplyController::getTargetId() const {
    return (6 << 8) | (dev_id << 2); // Адресная посылка (Приоритет 6)
    // return 0b11001011000;
}

bool PowerSupplyController::isMyReply(uint32_t can_id) const {
    // Базовый ID ответа: Приоритет 7 (111) и dev_id. Младшие 2 бита равны 00.
    uint32_t base_reply_id = (7 << 8) | (dev_id << 2);
    
    // Маска 0x7FC (0b11111111100) оставляет старшие 9 бит и обнуляет 2 младших.
    // Таким образом мы игнорируем 2 младших бита при сравнении.
    return (can_id & 0x7FC) == base_reply_id;
}

void PowerSupplyController::setPowerState(bool turnOn) {
    // F9 - Выходной регистр: бит 0 = ВКЛ (Контакты 30,12), бит 1 = ВЫКЛ (Контакты 31,13)
    if (!can) {
        emit logMessage("Power Control: Warning! CAN is not initialized.");
        return;
    }
    if (isBusy) {
        emit logMessage("Power Control: Warning! VCH-300 is busy, management is inaccessible.");
        return;
    }

    isBusy = true;
    emit deviceBusyStateChanged(true);

    uint8_t state = turnOn ? 0x01 : 0x02; 

    can->sendCommand(getTargetId(), 0xF9, {state});
    QTimer::singleShot(100, this, [this, turnOn]() {
        if (!can) {
            emit logMessage("Power Control: Warning! CAN is not initialized.");
            isBusy = false;
            emit deviceBusyStateChanged(false);
            return;
        }
        
        can->sendCommand(getTargetId(), 0xF9, {0x00});
        
        QTimer::singleShot(100, this, [this, turnOn]() {
            isBusy = false;
            emit deviceBusyStateChanged(false);
            emit logMessage(turnOn ? "PowerControl: Command power on." : "PowerControl: Command power off.");
        });
    });
}

void PowerSupplyController::setCurrent(float amperes) {
    // 80 - Запись ЦАП. 24-битный формат. FFFFF8 = 10В. (Предполагаем 8В = 300А на CLM-1000)
    if (!can) {
        emit logMessage("Power Control: Warning! CAN is not initialized.");
        return;
    }
    if (isBusy) {
        emit logMessage("Power Control: Warning! VCH-300 is busy, management is inaccessible.");
        return;
    }

    const double DAC_ZERO = 0x7FFFFC;      // 0 В
    const double DAC_FULL = 0xFFFFF8;      // +10 В
    const double VOLTAGE_RANGE = 20.0;     // от -10 до +10

    float voltage = (amperes / 300.0f) * 8.0f;
    if (voltage > 8.0f) voltage = 8.0f;
    if (voltage < 0) voltage = 0;
    
    uint32_t dac_code = static_cast<uint32_t>(DAC_ZERO + voltage * (DAC_FULL / VOLTAGE_RANGE));
    
    can->sendCommand(getTargetId(), 0x80, {
        static_cast<uint8_t>((dac_code >> 16) & 0xFF),
        static_cast<uint8_t>((dac_code >> 8) & 0xFF),
        static_cast<uint8_t>(dac_code & 0xFF),
        0x00, 0x00, 0x00 
    });
    emit logMessage(QString("Power Control: Setting current %1 А").arg(amperes));
}

void PowerSupplyController::resetProtection() {
    if (!can) {
        emit logMessage("Power Control: Warning! CAN is not initialized.");
        return;
    }
    if (isBusy) {
        emit logMessage("Power Control: Warning! VCH-300 is busy, management is inaccessible.");
        return;
    }

    isBusy = true;
    emit deviceBusyStateChanged(true);
    emit logMessage("Power Control: Resetting...");
    can->sendCommand(getTargetId(), 0xF9, {0x08});
    
    QTimer::singleShot(100, this, [this]() {
        if (!can) {
            emit logMessage("Power Control: Warning! CAN is not initialized.");
            isBusy = false;
            emit deviceBusyStateChanged(false);
            return;
        }
        
        can->sendCommand(getTargetId(), 0xF9, {0x00});
        
        QTimer::singleShot(2000, this, [this]() {
            isBusy = false;
            emit deviceBusyStateChanged(false);
            emit logMessage("Power Control: Reset completed.");
        });
    });
}

void PowerSupplyController::requestRegisters() {
    if (!can) return;
    can->sendCommand(getTargetId(), 0xF8, {});
}

void PowerSupplyController::processADCData(const CAN_PACKET& rcv) {
    if (rcv.len >= 5) {
        uint32_t adc_code = (rcv.data[4] << 16) | (rcv.data[3] << 8) | rcv.data[2];

        // sign extend 24-bit
        if (adc_code & 0x800000) {
            adc_code |= 0xFF000000;
        }

        // Кастуем к int32_t, чтобы отрицательные значения не превратились в мусор
        voltage_actual = (static_cast<int32_t>(adc_code) / static_cast<float>(0x3FFFFF)) * 10.0f;
        
        float clamped_voltage = voltage_actual < 0.0f ? 0.0f : voltage_actual;
        current_actual = (clamped_voltage / 8.0f) * 300.0f; 
    }
}

void PowerSupplyController::processRegisterData(const CAN_PACKET& rcv) {
    if (rcv.len >= 3) {
        status_flags = rcv.data[2]; 
    }

    // Выключение ВЧ-300 при наличии ошибки во входном регистре
    if (status_flags & 0x3E) {
        can->sendCommand(getTargetId(), 0xF9, {0x02});
    }
}

//           1000 0000 0000 0000 0000 0000 - 0x800000
// 1111 1111 0000 0000 0000 0000 0000 0000 - 0xFF000000
//           0011 1111 1111 1111 1111 1111 - 0x3FFFFF (+10V)
//           1100 0000 0000 0000 0000 0000 - 0xC00000 (-10V)