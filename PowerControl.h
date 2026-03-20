#ifndef POWERCONTROL_H
#define POWERCONTROL_H

#include <cstdint>
#include <vector>
#include <QObject>
#include "CanBusManager.h"

class PowerSupplyController : public QObject {
    Q_OBJECT
public:
    explicit PowerSupplyController(uint8_t deviceId = 1, QObject* parent = nullptr);
    ~PowerSupplyController() = default;

    void setCanInterface(CanBusManager* can_interface);

    void setPowerState(bool turnOn);
    void setCurrent(float amperes);
    void emergencyStop();
    
    void requestData();
    void processCanPacket(const CAN_PACKET& rcv);

    float getCurrent() const { return current_actual; }
    uint8_t getStatusFlags() const { return status_flags; }

signals:
    void logMessage(const QString& msg);

private:
    CanBusManager* can;
    uint8_t dev_id;
    float current_actual;
    uint8_t status_flags;

    uint32_t getTargetId() const;
};

#endif