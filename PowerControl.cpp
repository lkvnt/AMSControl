#include "PowerControl.h"
#include <iostream>
#include <windows.h>
#include "Pci7841.h"

PowerSupplyController::PowerSupplyController(uint8_t deviceId) 
    : card_handle(-1), dev_id(deviceId), current_actual(0.0f), status_flags(0) {std::cout << "Power here\n";}

PowerSupplyController::~PowerSupplyController() {
    if (card_handle >= 0) {
        setPowerState(false);
        CanCloseDriver(card_handle);
    }
}

bool PowerSupplyController::init() {
    std::cout << "Init has began\n";
    card_handle = CanOpenDriver(0, 0); // Карта 0, Порт 0
    if (card_handle == -1) {
        std::cout << "Driver is not open\n";
        return false;
    }
    if (card_handle != -1) {
        std::cout << "Driver is open\n";
    }

    PORT_STRUCT port_cfg;
    port_cfg.mode = 0;              // 11-bit CAN 2.0A
    port_cfg.accCode = 0;
    port_cfg.accMask = 0x7FF;       // Принимать все пакеты
    port_cfg.baudrate = 2;          // 500 Kbps
    
    // TODO: Сделать условия на эти методы чтобы проверить что все установилось и открылось
    if (CanConfigPort(card_handle, &port_cfg) == -1) {
        std::cout << "Config error\n";
        return false;
    }
    if (CanConfigPort(card_handle, &port_cfg) == -1) {
        std::cout << "Config ok\n";
    }
    CanEnableReceive(card_handle);
    return true; 
}

// Не оч понял зачем это надо
uint32_t PowerSupplyController::getTargetId() const {
    return (6 << 8) | (dev_id << 2); // Адресная посылка (Приоритет 6)
}

void PowerSupplyController::sendCanCommand(uint8_t cmd, const std::vector<uint8_t>& payload) {
    if (card_handle < 0) return;
    
    CAN_PACKET pkg = {0};
    pkg.CAN_ID = getTargetId();
    pkg.rtr = 0;
    pkg.len = payload.size() + 1;
    pkg.data[0] = cmd;
    for (size_t i = 0; i < payload.size() && i < 7; ++i) {
        pkg.data[i + 1] = payload[i];
    }
    CanSendMsg(card_handle, &pkg);
}

void PowerSupplyController::setPowerState(bool turnOn) {
    // F9 - Выходной регистр: бит 0 = ВКЛ (Контакты 30,12), бит 1 = ВЫКЛ (Контакты 31,13)
    uint8_t state = turnOn ? 0x01 : 0x02; 
    sendCanCommand(0xF9, {state});
}

void PowerSupplyController::setCurrent(float amperes) {
    // 80 - Запись ЦАП. 24-битный формат. FFFFF8 = 10В. (Предполагаем 8В = 300А на CLM-1000)
    float voltage = (amperes / 300.0f) * 8.0f;
    if (voltage > 8.0f) voltage = 8.0f;
    if (voltage < -8.0f) voltage = -8.0f;
    
    uint32_t dac_code = static_cast<uint32_t>((voltage / 10.0f) * 0xFFFFF8);
    
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>((dac_code >> 16) & 0xFF),
        static_cast<uint8_t>((dac_code >> 8) & 0xFF),
        static_cast<uint8_t>(dac_code & 0xFF),
        0x00, 0x00, 0x00 // Младшие байты игнорируются при прямой записи
    };
    sendCanCommand(0x80, payload);
}

void PowerSupplyController::requestActualCurrent() {
    // 02 - Запрос осциллографа (однократный). Канал 0, Time=0, Mode=0x20 (Выдача в линию)
    sendCanCommand(0x02, {0x00, 0x00, 0x20});
}

void PowerSupplyController::requestStatus() {
    sendCanCommand(0xF8, {}); // Чтение регистров
}

void PowerSupplyController::emergencyStop() {
    setPowerState(false);
    setCurrent(0.0f);
    std::cout << "[Power] !!! POWER IS OFF !!!" << std::endl;
}

void PowerSupplyController::processCanMessages() {
    if (card_handle < 0) return;
    
    requestActualCurrent();
    requestStatus();

    CAN_PACKET rcv;
    while (CanRcvMsg(card_handle, &rcv) == 0) {
        uint32_t reply_id = (7 << 8) | (dev_id << 2);
        if (rcv.CAN_ID != reply_id) continue;

        if (rcv.data[0] == 0x02 && rcv.len >= 5) {
            // Разбор АЦП (Код 3FFFFF = 10В)
            uint32_t adc_code = (rcv.data[4] << 16) | (rcv.data[3] << 8) | rcv.data[2];
            float voltage = (adc_code / static_cast<float>(0x3FFFFF)) * 10.0f;
            current_actual = (voltage / 8.0f) * 300.0f; 
        } 
        else if (rcv.data[0] == 0xF8 && rcv.len >= 3) {
            // Разбор входного регистра (data[2])
            status_flags = rcv.data[2]; 
        }
    }
}
