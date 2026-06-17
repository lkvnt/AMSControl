#include <QString>
#include "src/core/SettingsManager.h"
#include "src/hardware/CanBusManager.h"

CanBusManager::CanBusManager(QObject* parent) : QObject(parent), card_handle(-1) {
    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, &CanBusManager::pollCanBus);
}

CanBusManager::~CanBusManager() {
    CanClearRxBuffer(card_handle);
    CanClearTxBuffer(card_handle);
    close();
}

bool CanBusManager::init(int card, int port) {
    emit logMessage("CAN: Init started...");
    
    card_handle = CanOpenDriver(card, port);
    if (card_handle == -1) {
        emit logMessage("CAN: Error! PCI-7841 driver is not open.");
        return false;
    }

    PORT_STRUCT port_cfg;
    port_cfg.mode = 0;              
    port_cfg.accCode = 0;
    port_cfg.accMask = 0x7FF;       
    port_cfg.baudrate = 0; // 125 Kbps         
    
    if (CanConfigPort(card_handle, &port_cfg) != 0) {
        emit logMessage("CAN: Port config error!");
        close();
        return false;
    }
    
    CanEnableReceive(card_handle);
    emit logMessage("CAN: Init success.");

    int pollTimerValue = SettingsManager::instance().get("can_bus_poll_timer").toInt();
    pollTimer->start(pollTimerValue);

    return true;
}

void CanBusManager::close() {
    pollTimer->stop();
    if (card_handle >= 0) {
        emit logMessage("CAN: Port closing...");
        CanCloseDriver(card_handle);
        card_handle = -1;
    }
}

bool CanBusManager::sendCommand(uint32_t target_id, const std::vector<uint8_t>& payload) {
    if (card_handle < 0) return false;
    
    CAN_PACKET pkg = {0};
    pkg.CAN_ID = target_id;
    pkg.rtr = 0;
    pkg.len = payload.size();
    for (size_t i = 0; i < payload.size() && i < 8; ++i) {
        pkg.data[i] = payload[i];
    }
    return CanSendMsg(card_handle, &pkg) == 0;
}

bool CanBusManager::sendCommand(uint32_t target_id, uint8_t cmd, const std::vector<uint8_t>& payload) {
    if (card_handle < 0) return false;
    
    CAN_PACKET pkg = {0};
    pkg.CAN_ID = target_id;
    pkg.rtr = 0;
    pkg.len = payload.size() + 1;
    pkg.data[0] = cmd;
    for (size_t i = 0; i < payload.size() && i < 7; ++i) {
        pkg.data[i + 1] = payload[i];
    }
    return CanSendMsg(card_handle, &pkg) == 0;
}

void CanBusManager::handleUnknownPacket(const CAN_PACKET& pkt, const QString& prefix) {
    QString hexData;
    uint64_t data = 0;
    for (int i = 0; i < pkt.len; ++i) {
        hexData += QString("%1 ").arg(pkt.data[i], 2, 16, QChar('0')).toUpper();
    }
    QString message = prefix + QString(": Received unexpected data. ID: 0x%1 Data(HEX): %2").arg(QString::number(pkt.CAN_ID, 16).toUpper(), hexData.trimmed());
    emit logMessage(message);
}

void CanBusManager::pollCanBus() {
    if (card_handle < 0) return;
    
    CAN_PACKET pkt;
    while (CanRcvMsg(card_handle, &pkt) == 0) {
        emit packetReceived(pkt);
    }
}
