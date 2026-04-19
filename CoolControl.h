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
    float getTemperature() const { return 24.5f; }
    float getFlowRate() const { return 12.8f; }
    void emergencyStop();

signals:
    void logMessage(const QString& msg);
};

#endif