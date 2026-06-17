#include <QDateTime>
#include "src/core/SettingsManager.h"
#include "src/hardware/PowerControl.h"

#define PRIORITY_SEND 0b110
#define PRIORITY_RECEIVE 0b111

enum Command {
    STOP_MEASURE = 0x00,
    START_MEASURE = 0x02,
    SET_DAC = 0x80,
    ASK_REGISTER = 0xF8,
    SET_REGISTER = 0xF9,
    CHECK_CONNECT = 0xFF
};

PowerSupplyController::PowerSupplyController(QObject* parent) 
    : QObject(parent), can(nullptr), current_actual(0.0f), voltage_actual(0.0f), status_flags(0), isBusy(false) 
{
        lastMsgTime = QDateTime::currentMSecsSinceEpoch();
        dev_id = SettingsManager::instance().get("power_deviceId").toInt();
}

void PowerSupplyController::setCanInterface(CanBusManager* can_interface) {
    if (can) {
        emit logMessage("Power Control: CAN is already connected.");
        return;
    }
    can = can_interface;
    connect(can, &CanBusManager::packetReceived, this, &PowerSupplyController::handleMessage);
    emit logMessage("Power Control: CAN interface connected.");
}

uint32_t PowerSupplyController::getTargetId() const {
    return (PRIORITY_SEND << 8) | (dev_id << 2);
}

bool PowerSupplyController::isMyReply(uint32_t can_id) const {
    uint32_t base_reply_id = (PRIORITY_RECEIVE << 8) | (dev_id << 2);
    return (can_id & 0x7FC) == base_reply_id;
}

void PowerSupplyController::setPowerState(bool turnOn) {
    if (!can) {
        emit logMessage("Power Control: Warning! CAN is not initialized. Cannot set power state.");
        return;
    }
    if (isBusy) {
        emit logMessage("Power Control: Warning! VCH-300 is busy, power changing is inaccessible.");
        return;
    }

    emit logMessage(turnOn ? "Power Control: Command power on." : "Power Control: Command power off.");
    isBusy = true;
    emit deviceBusyStateChanged(isBusy);

    uint8_t state = turnOn ? 0x03 : 0x00; // Чтобы включить замыкается ВКЛ (0b01) и ВЫКЛ (0b10)

    can->sendCommand(getTargetId(), Command::SET_REGISTER, {state});

    QTimer::singleShot(300, this, [this, turnOn]() {
        isBusy = false;
        emit deviceBusyStateChanged(isBusy);
    });
}

void PowerSupplyController::setCurrent(float amperes) {
    // Запись ЦАП 24-битный формат. 7FFFFC = 0В FFFFF8 = 10В
    if (!can) {
        emit logMessage("Power Control: Warning! CAN is not initialized. Cannot set current.");
        return;
    }
    if (isBusy) {
        emit logMessage("Power Control: Warning! VCH-300 is busy, current setting is inaccessible.");
        return;
    }

    const double DAC_ZERO = 0x7FFFFC;
    const double DAC_FULL = 0xFFFFF8;
    const double VOLTAGE_RANGE = 20.0;

    float voltage = (amperes / 300.0f) * 8.0f;
    if (voltage > 8.0f) voltage = 8.0f;
    if (voltage < 0) voltage = 0;
    
    uint32_t dac_code = static_cast<uint32_t>(DAC_ZERO + voltage * (DAC_FULL / VOLTAGE_RANGE));
    
    can->sendCommand(getTargetId(), Command::SET_DAC, {
        static_cast<uint8_t>((dac_code >> 16) & 0xFF),
        static_cast<uint8_t>((dac_code >> 8) & 0xFF),
        static_cast<uint8_t>(dac_code & 0xFF),
        0x00, 0x00, 0x00 
    });
    emit logMessage(QString("Power Control: Setting current %1 А").arg(amperes));
}

void PowerSupplyController::resetProtection() {
    if (!can) {
        emit logMessage("Power Control: Warning! CAN is not initialized. Cannot reset protection.");
        return;
    }
    if (isBusy) {
        emit logMessage("Power Control: Warning! VCH-300 is busy, management is inaccessible.");
        return;
    }

    isBusy = true;
    emit deviceBusyStateChanged(isBusy);
    emit logMessage("Power Control: Resetting...");
    can->sendCommand(getTargetId(), Command::SET_REGISTER, {0x08});
    
    QTimer::singleShot(100, this, [this]() {
        if (!can) {
            emit logMessage("Power Control: Warning! CAN is not initialized. Cannot clear register from reset protection.");
            isBusy = false;
            emit deviceBusyStateChanged(isBusy);
            return;
        }
        
        can->sendCommand(getTargetId(), Command::SET_REGISTER, {0x00});
        
        QTimer::singleShot(300, this, [this]() {
            isBusy = false;
            emit deviceBusyStateChanged(isBusy);
            emit logMessage("Power Control: Reset completed.");
        });
    });
}

void PowerSupplyController::requestRegisters() {
    if (!can) {
        emit logMessage("Power Control: Warning! CAN is not initialized. Cannot request registers.");
        return;
    }
    can->sendCommand(getTargetId(), Command::ASK_REGISTER, {});
}

void PowerSupplyController::processADCData(const CAN_PACKET& rcv) {
    if (rcv.len >= 5) {
        uint32_t adc_code = (rcv.data[4] << 16) | (rcv.data[3] << 8) | rcv.data[2];

        if (adc_code & 0x800000) {
            adc_code |= 0xFF000000;
        }

        voltage_actual = (static_cast<int32_t>(adc_code) / static_cast<float>(0x3FFFFF)) * 10.0f;
        
        float clamped_voltage = voltage_actual < 0.0f ? 0.0f : voltage_actual;
        current_actual = (clamped_voltage / 8.0f) * 300.0f; 
    }
}

void PowerSupplyController::requestConnection() {
    if (!can) {
        emit logMessage("Power Control: Warning! CAN is not initialized. Cannot request connection.");
        return;
    }
    can->sendCommand(getTargetId(), {Command::CHECK_CONNECT});
}

void PowerSupplyController::requestDataFlow() {
    if (!can) {
        emit logMessage("Power Control: Warning! CAN is not initialized. Cannot request data flow.");
        return;
    }
    can->sendCommand(getTargetId(), Command::START_MEASURE, {0x00, 0x07, 0x30}); // {CHANNEL, PERIOD, ONCE=0x20/FLOW=0x30}
}

void PowerSupplyController::stopDataFlow() {
    if (!can) {
        emit logMessage("Power Control: Warning! CAN is not initialized. Cannot stop data flow.");
        return;
    }
    can->sendCommand(getTargetId(), {Command::STOP_MEASURE});
}

void PowerSupplyController::processRegisterData(const CAN_PACKET& rcv) {
    if (rcv.len >= 3) {
        status_flags = rcv.data[2]; 
    }

    if (status_flags & REGISTER_HAS_ERROR) {
        can->sendCommand(getTargetId(), Command::SET_REGISTER, {0x00});
    }
}

std::pair<bool, QString> PowerSupplyController::messageFromRegister(uint8_t reg) const {
    if (reg == 0x00) return {false, "VCH-300 is off!"};
    if (reg == 0x01) return {true, "VCH-300 is on."};

    if (reg & 0xC0) return {false, "Unknown power status!"};

    QString outMsg = "";
    if (reg & 0x02) outMsg += "Power out protection 1!";
    if (reg & 0x04) outMsg += "Power out protection 2!";
    if (reg & 0x08) outMsg += "Power temperature protection!";
    if (reg & 0x10) outMsg += "Power invertor error!";
    if (reg & 0x20) outMsg += "Power phases error!";
    return {false, outMsg};
}

void PowerSupplyController::handleMessage(const CAN_PACKET& pkt) {
    if (isMyReply(pkt.CAN_ID)) {
        lastMsgTime = QDateTime::currentMSecsSinceEpoch();
        uint8_t cmd = pkt.data[0];
        if (cmd == Command::START_MEASURE) {
            processADCData(pkt);
        } else if (cmd == Command::ASK_REGISTER) {
            processRegisterData(pkt);
        } else if (cmd == Command::CHECK_CONNECT) {
            cdacResponded = true;
        } else {
            can->handleUnknownPacket(pkt, "Power Control");
        }
    }
}

void PowerSupplyController::onSystemStop() {
    isBusy = false;
    setCurrent(0);
    setPowerState(false);
    cdacResponded = false;
}
