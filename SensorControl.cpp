#include <cmath>
#include "SensorControl.h"
#include "SettingsManager.h"

SensorController::SensorController(QObject* parent) 
    : QObject(parent), can(nullptr), faraday_v(0), hall_v(0), vacuum_v(0) {
        dev_id = SettingsManager::instance().get("power_deviceId").toInt();
    }

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
    SettingsManager &sm = SettingsManager::instance();
    if (rcv.len >= 5) {
        uint8_t channel = rcv.data[1]; // У CAC208 канал передается в data[1]
        uint32_t adc_code = (rcv.data[4] << 16) | (rcv.data[3] << 8) | rcv.data[2];

        // sign extend 24-bit
        if (adc_code & 0x800000) {
            adc_code |= 0xFF000000;
        }

        // Базовое преобразование +-10В
        float voltage = (static_cast<int32_t>(adc_code) / static_cast<float>(0x3FFFFF)) * 10.0f;

        uint8_t chHall = sm.get("sensor_chanHall").toInt();
        uint8_t chFaraday = sm.get("sensor_chanFaraday").toInt();
        uint8_t chVacuum = sm.get("sensor_chanVacuum").toInt();
        // Распределяем по переменным в зависимости от канала
        if (channel == chHall) {
            hall_v = voltage; // Датчик Холла (INM17, INP17)
        } 
        else if (channel == chFaraday) {
            faraday_v = voltage;    // Цилиндр Фарадея (INM18, INP18)
        } 
        else if (channel == chVacuum) {
            vacuum_v = voltage;  // ВМБ-14 вакуум (INM19, INP19)
        }
    }
}

float SensorController::getPressFromVolt(float volt) {
    if (volt > 1e-3 && volt < 6.7835f) return exp((volt - 11.2128f) / 0.7124f);
    else if (volt >= 6.7835f && volt < 8.0272f) return exp((volt - 10.9212f) / 0.6655f);
    else if (volt >= 8.0272f && volt < 8.9702f) return exp((volt - 9.9684f) / 0.4464f);
    else if (volt >= 8.9702f && volt <= 9.5f) return exp((volt - 9.5545f) / 0.2613f);
    else return -1;
}

void SensorController::handleMessage(const CAN_PACKET& pkt) {
    uint8_t cmd = pkt.data[0];
    if (cmd == 0x01) {
        processADCData(pkt);
    } else {
        QString hexData;
        uint64_t data = 0;
        for (int i = 0; i < pkt.len; ++i) {
            hexData += QString("%1 ").arg(pkt.data[i], 2, 16, QChar('0')).toUpper();
        }
        emit logMessage(QString("SystemManager: Received unexpected data. ID: 0x%1 Data(HEX): %2").arg(QString::number(pkt.CAN_ID, 16).toUpper(), hexData.trimmed()));
    }
}