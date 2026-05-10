#include <cmath>
#include "SensorControl.h"

SensorController::SensorController(uint8_t deviceId, QObject* parent) 
    : QObject(parent), can(nullptr), dev_id(deviceId), faraday_v(0), hall_v(0), vacuum_v(0) {}

void SensorController::setCanInterface(CanBusManager* can_interface) {
    can = can_interface;
    emit logMessage("SensorControl: CAN interface connected.");
}

uint32_t SensorController::getTargetId() const {
    return (6 << 8) | (dev_id << 2);
}

bool SensorController::isMyReply(uint32_t can_id) const {
    uint32_t base_reply_id = (7 << 8) | (dev_id << 2);
    return (can_id & 0x7FC) == base_reply_id;
}

void SensorController::processADCData(const CAN_PACKET& rcv) {
    if (rcv.len >= 5) {
        uint8_t channel = rcv.data[1]; // У CAC208 канал передается в data[1]
        uint32_t adc_code = (rcv.data[4] << 16) | (rcv.data[3] << 8) | rcv.data[2];

        // sign extend 24-bit
        if (adc_code & 0x800000) {
            adc_code |= 0xFF000000;
        }

        // Базовое преобразование +-10В
        float voltage = (static_cast<int32_t>(adc_code) / static_cast<float>(0x3FFFFF)) * 10.0f;

        // Распределяем по переменным в зависимости от канала
        if (channel == 0x11) {
            hall_v = voltage; // Датчик Холла (INM17, INP17)
        } 
        else if (channel == 0x12) {
            faraday_v = voltage;    // Цилиндр Фарадея (INM18, INP18)
        } 
        else if (channel == 0x13) {
            vacuum_v = voltage;  // ВМБ-14 вакуум (INM19, INP19)
        }
    }
}

float SensorController::getPressFromVolt(float volt) {
    if (volt > 0 && volt < 6.7835f) return 11.2128f + 0.7124 * std::log(volt);
    else if (volt >= 6.7835f && volt < 8.0272f) return 10.9212 + 0.6655 * std::log(volt);
    else if (volt >= 8.0272f && volt < 8.9702f) return 9.9684 + 0.4464 * std::log(volt);
    else if (volt >= 8.9702f && volt <= 9.5f) return 9.5545 + 0.2613 * std::log(volt);
    else return -1;
}