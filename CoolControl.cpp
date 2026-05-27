#include "CoolControl.h"
#include "SettingsManager.h"

#define PRIORITY_SEND 0b110
#define PRIORITY_RECEIVE 0b111

enum Command {
    STOP_MEASURE = 0x00,
    START_MEASURE = 0x01,
    SET_STATE = 0x02,
    CHECK_CONNECT = 0xFF
};

CoolingController::CoolingController(QObject* parent)
    : QObject(parent), can(nullptr), waterFlow(0), pumpState(false), coolState(false) {
    dev_id = SettingsManager::instance().get("cool_deviceId").toInt();
}

void CoolingController::setCanInterface(CanBusManager* can_interface) {
    if (can) {
        emit logMessage("CoolControl: CAN is already connected.");
    }
    can = can_interface;
    emit logMessage("CoolControl: CAN interface connected.");
}

void CoolingController::setPumpState(bool start) {
    if (!can) {
        emit logMessage("CoolControl: Warning! CAN is not initialized.");
        return;
    }
    uint8_t state = start ? 0x01 : 0x00;
    can->sendCommand(getTargetId(), Command::SET_STATE, {state});
}

void CoolingController::setCoolerState(bool start) {
    coolState = start ? true : false;
    emit logMessage(start ? "Cooling: Cooler is on" : "Cooling: Cooler is off.");
}

float CoolingController::getTemperature() const {
    return SettingsManager::instance().get("cool_mockTemp").toFloat();
}

float CoolingController::getFlowRate() const {
    return waterFlow / 100.0f;
}

bool CoolingController::isMyReply(uint32_t can_id) const {
    uint32_t base_reply_id = (PRIORITY_RECEIVE << 8) | (dev_id << 2);
    return (can_id & 0x7FC) == base_reply_id;
}

void CoolingController::handleMessage(const CAN_PACKET& pkt) {
    if (pkt.data[0] == Command::START_MEASURE) {
        waterFlow = (pkt.data[1] << 8) | (pkt.data[2]);
    } else if (pkt.data[0] == Command::SET_STATE) {
        pumpState = (pkt.data[1] == 0x01);
    }
}

uint32_t CoolingController::getTargetId() const {
    return (PRIORITY_SEND << 8) | (dev_id << 2);
}

void CoolingController::requestConnection() {
    if (!can) {
        emit logMessage("CoolControl: Warning! CAN is not initialized.");
        return;
    }
    can->sendCommand(getTargetId(), {Command::CHECK_CONNECT});
}

void CoolingController::requestDataFlow() {
    if (!can) {
        emit logMessage("CoolControl: Warning! CAN is not initialized.");
        return;
    }
    can->sendCommand(getTargetId(), {Command::START_MEASURE});
}

void CoolingController::stopDataFlow() {
    if (!can) {
        emit logMessage("CoolControl: Warning! CAN is not initialized.");
        return;
    }
    can->sendCommand(getTargetId(), {Command::STOP_MEASURE});
}