#ifndef CANBUSMANAGER_H
#define CANBUSMANAGER_H

#include <QObject>
#include <QTimer>
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

    bool isOpen() const { return card_handle != -1; }

signals:
    void logMessage(const QString& msg);
    void packetReceived(const CAN_PACKET& pkt);

private slots:
    void pollCanBus();

private:
    int card_handle;
    QTimer* pollTimer;
};

#endif // CANBUSMANAGER_H