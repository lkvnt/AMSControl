#ifndef POWERCONTROL_H
#define POWERCONTROL_H

#include <cstdint>
#include <vector>
#include <QObject>
#include "CanBusManager.h"

class PowerSupplyController : public QObject {
    Q_OBJECT
public:
    explicit PowerSupplyController(QObject* parent = nullptr);
    ~PowerSupplyController() = default;

    void setCanInterface(CanBusManager* can_interface);

    void setPowerState(bool turnOn);
    void setCurrent(float amperes);
    void resetProtection();

    void requestConnection();
    void requestRegisters();
    void requestDataFlow();
    void stopDataFlow();

    void processADCData(const CAN_PACKET& rcv);
    void processRegisterData(const CAN_PACKET& rcv);

    void handleMessage(const CAN_PACKET& pkt);

    float getCurrent() const { return current_actual; }
    float getAdcVoltage() const { return voltage_actual; }
    uint8_t getStatusFlags() const { return status_flags; }

    uint32_t getTargetId() const;
    bool isMyReply(uint32_t can_id) const;
    bool isDeviceBusy() const { return isBusy; }

    const uint8_t REGISTER_HAS_ERROR = 0x3E;

signals:
    void logMessage(const QString& msg);
    void deviceBusyStateChanged(bool isBusy);

private:
    CanBusManager* can;
    uint8_t dev_id;
    float current_actual;
    float voltage_actual;
    uint8_t status_flags;
    bool isBusy;
};

#endif