#include "CanBusManager.h"
#include <QString>

CanBusManager::CanBusManager(QObject* parent) : QObject(parent), card_handle(-1) {}

CanBusManager::~CanBusManager() {
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
        emit logMessage("CAN: Port config error.");
        close();
        return false;
    }
    
    CanEnableReceive(card_handle);
    emit logMessage("CAN: Init success.");
    return true;
}

void CanBusManager::close() {
    if (card_handle >= 0) {
        emit logMessage("CAN: Port closing...");
        CanCloseDriver(card_handle);
        card_handle = -1;
    }
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

bool CanBusManager::receivePacket(CAN_PACKET& out_packet) {
    if (card_handle < 0) return false;
    return CanRcvMsg(card_handle, &out_packet) == 0;
}