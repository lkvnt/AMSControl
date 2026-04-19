#ifndef SENSORCONTROL_H
#define SENSORCONTROL_H

#include <cstdint>
#include <QObject>
#include "CanBusManager.h"

class SensorController : public QObject {
    Q_OBJECT
public:
    // Поменять на ID реальный
    explicit SensorController(uint8_t deviceId = 0b010111, QObject* parent = nullptr);
    ~SensorController() = default;

    void setCanInterface(CanBusManager* can_interface);

    // Обработка пакета
    void processADCData(const CAN_PACKET& rcv);

    // Геттеры
    float getFaradayVoltage() const { return faraday_v; }
    float getHallVoltage() const { return hall_v; }
    float getVacuumVoltage() const { return vacuum_v; }

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