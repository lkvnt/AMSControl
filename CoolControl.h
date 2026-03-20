#ifndef COOLCONTROL_H
#define COOLCONTROL_H

#include <cstdint>
#include <QObject>

class CoolingController : public QObject {
    Q_OBJECT
public:
    CoolingController();
    void setPumpState(bool start);
    void setCoolerState(bool start);
    float getTemperature();
    float getFlowRate();
    void emergencyStop();

signals:
    void logMessage(const QString& msg);
};

#endif