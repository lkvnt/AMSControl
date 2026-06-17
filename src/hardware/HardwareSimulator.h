#ifndef HARDWARESIMULATOR_H
#define HARDWARESIMULATOR_H

#include <QWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>
#include "src/hardware/CanBusManager.h"


class SimulatorUI : public QWidget {
    Q_OBJECT
public:
    explicit SimulatorUI(QWidget *parent = nullptr);

public slots:
    void onTargetCurrentChanged(float amps);
    void onPumpStateChanged(bool state);
    void onPowerStateChanged(bool state);
    void onPowerFlagsChanged();

signals:
    void powerParamsChanged(float voltage, uint8_t flags, bool isOnline);
    void coolParamsChanged(int flow, bool isOnline);
    void sensorParamsChanged(float f1, float f2, float hall, float vac, bool isOnline);

private:
    QCheckBox *chkPowerOnline;
    QCheckBox *chkCoolOnline;
    QCheckBox *chkSensOnline;

    QDoubleSpinBox *powVoltSpin;
    QCheckBox *chkErrOut1, *chkErrOut2, *chkErrTemp, *chkErrInv, *chkErrPhase;
    QSpinBox *flowSpin;
    QDoubleSpinBox *sensF1Spin, *sensF2Spin, *sensHallSpin, *sensVacSpin;

    QLabel *lblTargetCur;
    QLabel *lblPumpState;
    QLabel *lblPowerState;

    void broadcastValues();
};


class VirtualCanBusManager : public CanBusManager {
    Q_OBJECT
public:
    explicit VirtualCanBusManager(QObject *parent = nullptr);
    ~VirtualCanBusManager();

    bool init(int card = 0, int port = 0) override;
    void close() override;
    bool sendCommand(uint32_t target_id, const std::vector<uint8_t>& payload) override;
    bool sendCommand(uint32_t target_id, uint8_t cmd, const std::vector<uint8_t>& payload) override;
    bool isOpen() const override { return true; }


public slots:
    void updatePower(float voltage, uint8_t flags, bool isOnline);
    void updateCool(int flow, bool isOnline);
    void updateSensors(float f1, float f2, float hall, float vac, bool isOnline);

signals:
    void notifyTargetCurrent(float amps);
    void notifyPumpState(bool state);
    void notifyPowerState(bool state);
    void notifyPowerFlags();

private slots:
    void onSimTick();

private:
    QTimer* simTimer;

    bool powerOnline = true;
    bool coolOnline = true;
    bool sensorOnline = true;

    bool simPowerData = false;
    bool simCoolData = false;
    bool simSensorData = false;

    float mockPowVolt = 0.0f;
    uint8_t mockPowFlags = 0;
    int mockFlow = 0;
    float mockF1 = 0, mockF2 = 0, mockHall = 0, mockVac = 0;

    uint32_t packAdc(float voltage);
};

#endif // HARDWARESIMULATOR_H