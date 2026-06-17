#ifndef COOLCONTROL_H
#define COOLCONTROL_H

#include <cstdint>
#include <QObject>
#include "src/hardware/CanBusManager.h"

class CoolingController : public QObject {
    Q_OBJECT
public:
    explicit CoolingController(QObject* parent = nullptr);
    ~CoolingController() = default;

    void setCanInterface(CanBusManager* can_interface);

    void setState(bool start);

    float getTemperature() const;
    float getFlowRate() const;
    bool getState() const { return state; }
    bool isResponded() const { return arduinoResponded; }
    qint64 getLastMsgTime() const { return lastMsgTime; }

    bool isMyReply(uint32_t can_id) const;
    uint32_t getTargetId() const;

    void requestConnection();
    void requestDataFlow();
    void stopDataFlow();

private slots:
    void handleMessage(const CAN_PACKET& pkt);

public slots:
    void onSystemStop();

private:
    CanBusManager* can;
    bool state;
    uint16_t waterFlow;
    uint8_t dev_id;
    bool arduinoResponded = false;
    qint64 lastMsgTime;

signals:
    void logMessage(const QString& msg);
};

#endif