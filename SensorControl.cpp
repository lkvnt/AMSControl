#include <cmath>
#include "SensorControl.h"
#include "SettingsManager.h"

#define PRIORITY_SEND 0b110
#define PRIORITY_RECEIVE 0b111

enum Command {
    STOP_MEASURE = 0x00,
    START_MEASURE = 0x01,
    CHECK_CONNECT = 0xFF
};

SensorController::SensorController(QObject* parent) 
    : QObject(parent), can(nullptr), faraday_v(0), hall_v(0), vacuum_v(0) {
        dev_id = SettingsManager::instance().get("sensor_deviceId").toInt();
    }

void SensorController::setCanInterface(CanBusManager* can_interface) {
    if (can) {
        emit logMessage("SensorControl: CAN is already connected.");
    }
    can = can_interface;
    emit logMessage("SensorControl: CAN interface connected.");
}

void SensorController::requestDataFlow() {
    if (!can) {
        emit logMessage("SensorControl: Warning! CAN is not initialized.");
        return;
    }
    SettingsManager &sm = SettingsManager::instance();
    uint8_t chHall = sm.get("sensor_chanHall").toInt();
    uint8_t chFaraday = sm.get("sensor_chanFaraday").toInt();
    uint8_t chVacuum = sm.get("sensor_chanVacuum").toInt();
    can->sendCommand(getTargetId(), Command::START_MEASURE, { std::min({chHall, chFaraday, chVacuum}),
                                            std::max({chHall, chFaraday, chVacuum}),
                                            0x07, 0x30, 0x00});
}

void SensorController::requestConnection() {
    if (!can) {
        emit logMessage("SensorControl: Warning! CAN is not initialized.");
        return;
    }
    can->sendCommand(getTargetId(), {Command::CHECK_CONNECT});
}

void SensorController::stopDataFlow() {
    if (!can) {
        emit logMessage("SensorControl: Warning! CAN is not initialized.");
        return;
    }
    can->sendCommand(getTargetId(), {Command::STOP_MEASURE});
}

uint32_t SensorController::getTargetId() const {
    return (PRIORITY_SEND << 8) | (dev_id << 2);
}

bool SensorController::isMyReply(uint32_t can_id) const {
    uint32_t base_reply_id = (PRIORITY_RECEIVE << 8) | (dev_id << 2);
    return (can_id & 0x7FC) == base_reply_id;
}

void SensorController::processADCData(const CAN_PACKET& rcv) {
    SettingsManager &sm = SettingsManager::instance();
    if (rcv.len >= 5) {
        uint8_t channel = rcv.data[1];
        uint32_t adc_code = (rcv.data[4] << 16) | (rcv.data[3] << 8) | rcv.data[2];

        if (adc_code & 0x800000) {
            adc_code |= 0xFF000000;
        }

        float voltage = (static_cast<int32_t>(adc_code) / static_cast<float>(0x3FFFFF)) * 10.0f;

        uint8_t chHall = sm.get("sensor_chanHall").toInt();
        uint8_t chFaraday = sm.get("sensor_chanFaraday").toInt();
        uint8_t chVacuum = sm.get("sensor_chanVacuum").toInt();

        if (channel == chHall) {
            hall_v = voltage;
        } 
        else if (channel == chFaraday) {
            faraday_v = voltage;
        } 
        else if (channel == chVacuum) {
            vacuum_v = voltage;
        }
    }
}

std::optional<double> SensorController::getPressFromVolt(float volt) {
    if (volt > 1e-3 && volt < 6.7835f) return exp((volt - 11.2128f) / 0.7124f);
    else if (volt >= 6.7835f && volt < 8.0272f) return exp((volt - 10.9212f) / 0.6655f);
    else if (volt >= 8.0272f && volt < 8.9702f) return exp((volt - 9.9684f) / 0.4464f);
    else if (volt >= 8.9702f && volt <= 9.5f) return exp((volt - 9.5545f) / 0.2613f);
    else return std::nullopt;
}

void SensorController::handleMessage(const CAN_PACKET& pkt) {
    uint8_t cmd = pkt.data[0];
    if (cmd == Command::START_MEASURE) {
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