#ifndef MANAGER_H
#define MANAGER_H

#include <QObject>
#include "PowerControl.h"
#include "CoolControl.h"
#include "CanBusManager.h"
#include "SensorControl.h"
#include "Logger.h"
#include "LogWorker.h"

class SystemManager : public QObject {
    Q_OBJECT
public:
    explicit SystemManager(std::unique_ptr<Logger> eventLogger,
                           std::unique_ptr<Logger> telemetryLogger,
                           QObject* parent = nullptr);
    ~SystemManager();

    void initHardware();

    void startSystem();
    void stopSystem();
    
    void update(); 

    void setCurrent(float amperes, bool manual = false);

    float getTemp() const { return cooling.getTemperature(); }
    float getFlow() const { return cooling.getFlowRate(); }
    float getCurrent() const { return power.getCurrent(); }
    float getAdcVoltage() const { return power.getAdcVoltage(); }
    float getFaraday1() const { return sensors.getFaraday1Voltage(); }
    float getFaraday2() const { return sensors.getFaraday2Voltage(); }
    float getHall() const { return sensors.getHallVoltage(); }
    float getVacuum() const { return sensors.getVacuumVoltage(); }

    bool getCoolState() const { return cooling.getState(); }
    bool isOk() const { return is_running; }
    bool isBusy() const { return power.isDeviceBusy() || startup_step > 0; }
    uint8_t getStatusFlags() const { return power.getStatusFlags(); }

    void manualPowerOn();
    void manualPowerOff();
    void manualResetProt();
    void manualSetCurrent(float amperes);
    void manualCoolingOn();
    void manualCoolingOff();
    void manualRequestSensorData();

signals:
    void logMessage(const QString& msg);
    void busyStateChanged(bool isBusy);
    void stopSystemSignal();
    void requestEventLog(const QString& dirPath, const QString& fileName, const QVariant& data);
    void requestTelemetryLog(const QString& dirPath, const QString& fileName, const QVariant& data);

private slots:
    void onLogMessageReceived(const QString& msg);
    void onDataLogTimeout();

private:
    CanBusManager canBus;
    PowerSupplyController power;
    CoolingController cooling;
    SensorController sensors;
    
    bool is_running = false;
    volatile int startup_step = 0;
    
    void continueStartSystem(int step);

    QTimer *updateTimer;
    float updateFreq;
    void checkInterlocks(float flow, float temp, uint8_t power_status); // TODO: сделать в настройках выбор проверять или нет

    QTimer *dataLogTimer;
    QThread logThread;
    LogWorker *logWorker;
};

#endif // SYSTEM_MANAGER_H