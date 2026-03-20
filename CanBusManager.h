#ifndef CANBUSMANAGER_H
#define CANBUSMANAGER_H

#include <QObject>
#include <vector>
#include <cstdint>
#include <windows.h>
#include "Pci7841.h"

class CanBusManager : public QObject {
    Q_OBJECT
public:
    explicit CanBusManager(QObject* parent = nullptr);
    ~CanBusManager();

    bool init(int card = 0, int port = 0);
    void close();
    bool sendCommand(uint32_t target_id, uint8_t cmd, const std::vector<uint8_t>& payload);
    bool receivePacket(CAN_PACKET& out_packet);

    bool isOpen() const { return card_handle != -1; }

signals:
    void logMessage(const QString& msg);

private:
    int card_handle;
};

#endif // CANBUSMANAGER_H