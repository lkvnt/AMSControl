#ifndef COOLCONTROL_H
#define COOLCONTROL_H

#include <cstdint>
#include <QObject>
#include "CanBusManager.h"

class CoolingController : public QObject {
    Q_OBJECT
public:
    explicit CoolingController(QObject* parent = nullptr);
    ~CoolingController() = default;

    void setCanInterface(CanBusManager* can_interface);

    void setPumpState(bool start);
    void setCoolerState(bool start);

    float getTemperature() const;
    float getFlowRate() const;
    bool getPumpState() const { return pumpState; }
    bool getCoolState() const { return coolState; }

    bool isMyReply(uint32_t can_id) const;
    void handleMessage(const CAN_PACKET& pkt);
    uint32_t getTargetId() const;

    void requestConnection();
    void requestDataFlow();
    void stopDataFlow();

private:
    CanBusManager* can;
    bool pumpState;
    bool coolState;
    uint16_t waterFlow;
    uint8_t dev_id;

signals:
    void logMessage(const QString& msg);
};

#endif