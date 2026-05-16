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

    // Обработка пакета
    void processADCData(const CAN_PACKET& rcv);

    void handleMessage(const CAN_PACKET& pkt);

    // Геттеры
    float getFaradayVoltage() const { return faraday_v; }
    float getHallVoltage() const { return hall_v; }
    float getVacuumVoltage() const { return vacuum_v; }

    static float getPressFromVolt(float volt);

    uint32_t getTargetId() const;
    bool isMyReply(uint32_t can_id) const;

signals:
    void logMessage(const QString& msg);

private:
    CanBusManager* can;
    uint8_t dev_id;
    
    float faraday_v;
    float hall_v;
    float vacuum_v;
};

#endif // SENSORCONTROL_H