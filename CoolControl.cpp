#include "CoolControl.h"
#include "SettingsManager.h"
#include <iostream>

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
    can->sendCommand(getTargetId(), 0x02, {state});
    //pumpState = start ? true : false;
    //emit logMessage(start ? "Cooling: Pump is on" : "Cooling: Pump is off.");
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
    uint32_t base_reply_id = (7 << 8) | (dev_id << 2);
    return (can_id & 0x7FC) == base_reply_id;
}

void CoolingController::handleMessage(const CAN_PACKET& pkt) {
    if (pkt.data[0] == 0x01) {
        waterFlow = (pkt.data[1] << 8) | (pkt.data[2]);
    } else if (pkt.data[0] == 0x02) {
        pumpState = (pkt.data[1] == 0x01);
    }
}

uint32_t CoolingController::getTargetId() const {
    return (6 << 8) | (dev_id << 2);
}

void CoolingController::requestConnection() {
    if (!can) {
        emit logMessage("CoolControl: Warning! CAN is not initialized.");
        return;
    }
    can->sendCommand(getTargetId(), {0xFF});
}

void CoolingController::requestDataFlow() {
    if (!can) {
        emit logMessage("CoolControl: Warning! CAN is not initialized.");
        return;
    }
    can->sendCommand(getTargetId(), {0x01});
}

void CoolingController::stopDataFlow() {
    if (!can) {
        emit logMessage("CoolControl: Warning! CAN is not initialized.");
        return;
    }
    can->sendCommand(getTargetId(), {0x00});
}