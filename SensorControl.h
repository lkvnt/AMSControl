#ifndef SENSORCONTROL_H
#define SENSORCONTROL_H

#include <cstdint>
#include <QObject>
#include "CanBusManager.h"

class SensorController : public QObject {
    Q_OBJECT
public:
    explicit SensorController(QObject* parent = nullptr);
    ~SensorController() = default;

    void setCanInterface(CanBusManager* can_interface);

    void requestConnection();
    void requestDataFlow();
    void stopDataFlow();

    void processADCData(const CAN_PACKET& rcv);

    float getFaraday1Voltage() const { return faraday1_v; }
    float getFaraday2Voltage() const { return faraday2_v; }
    float getHallVoltage() const { return hall_v; }
    float getVacuumVoltage() const { return vacuum_v; }
    std::optional<double> getVacuumPressure() const { return vacuum_p; }
    bool isResponded() const { return cacResponded; }
    qint64 getLastMsgTime() const { return lastMsgTime; }

    static std::optional<double> getPressFromVolt(float volt);

    uint32_t getTargetId() const;
    bool isMyReply(uint32_t can_id) const;

signals:
    void logMessage(const QString& msg);

private slots:
    void handleMessage(const CAN_PACKET& pkt);

public slots:
    void onSystemStop();

private:
    CanBusManager* can;
    uint8_t dev_id;
    
    float faraday1_v;
    float faraday2_v;
    float hall_v;
    float vacuum_v;
    std::optional<double> vacuum_p;

    bool cacResponded = false;
    qint64 lastMsgTime;
};

#endif // SENSORCONTROL_H