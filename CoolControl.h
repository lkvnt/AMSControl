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
    float getTemperature() const;
    float getFlowRate() const;
    bool getPumpState() const { return pumpState; }
    bool getCoolState() const { return coolState; }

private:
    bool pumpState;
    bool coolState;

signals:
    void logMessage(const QString& msg);
};

#endif