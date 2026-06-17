#ifndef CANBUSMANAGER_H
#define CANBUSMANAGER_H

#include <QObject>
#include <QTimer>
#include <vector>
#include <cstdint>

// #define WIN32_LEAN_AND_MEAN
// #define NOMINMAX
// #include <windows.h>

#include "Pci7841.h"

class CanBusManager : public QObject {
    Q_OBJECT
public:
    explicit CanBusManager(QObject* parent = nullptr);
    virtual ~CanBusManager();

    virtual bool init(int card = 0, int port = 0);
    virtual void close();
    virtual bool sendCommand(uint32_t target_id, const std::vector<uint8_t>& payload);
    virtual bool sendCommand(uint32_t target_id, uint8_t cmd, const std::vector<uint8_t>& payload);
    void handleUnknownPacket(const CAN_PACKET& pkt, const QString& prefix);

    virtual bool isOpen() const { return card_handle != -1; }

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