#ifndef MANAGER_H
#define MANAGER_H

#include <QObject>
#include "PowerControl.h"
#include "CoolControl.h"
#include "CanBusManager.h"
#include "SensorControl.h"

/*
 * Класс SystemManager объединяет все подсистемы установки.
 * Реализует логику безопасности (Interlocks) и предоставляет единый интерфейс для UI.
 */
class SystemManager : public QObject {
    Q_OBJECT
public:
    explicit SystemManager(QObject* parent = nullptr);
    ~SystemManager();

    // Инициализация PCI-7841
    void initHardware();

    // --- Логика управления ---
    void startSystem();      // Запуск
    void stopSystem();       // Остановка
    
    // Метод для вызова в цикле таймера (проверка условий безопасности)
    void update(); 

    // --- Проброс команд к питанию ---
    void setCurrent(float amperes);

    // --- Геттеры для интерфейса (получение текущих значений) ---
    float getTemp() const { return cooling.getTemperature(); }
    float getFlow() const { return cooling.getFlowRate(); }
    float getCurrent() const { return power.getCurrent(); }
    float getAdcVoltage() const { return power.getAdcVoltage(); }
    float getFaraday() const { return sensors.getFaradayVoltage(); }
    float getHall() const { return sensors.getHallVoltage(); }
    float getVacuum() const { return sensors.getVacuumVoltage(); }

    // Состояние системы для индикации в UI
    bool getPumpState() const { return cooling.getPumpState(); }
    bool getCoolState() const { return cooling.getCoolState(); }
    bool isOk() const { return is_system_ok; }
    bool isBusy() const { return power.isDeviceBusy() || startup_step > 0; }
    uint8_t getStatusFlags() const { return power.getStatusFlags(); }

    // Ручное управление
    void manualPowerOn() { power.setPowerState(true); }
    void manualPowerOff() { power.setPowerState(false); }
    void manualResetProt() { power.resetProtection(); }
    void manualCoolingOn() { cooling.setPumpState(true); cooling.setCoolerState(true); }
    void manualCoolingOff() { cooling.setPumpState(false); cooling.setCoolerState(false); }

signals:
    void logMessage(const QString& msg);
    void busyStateChanged(bool isBusy);

private slots:
    void handleIncomingPacket(const CAN_PACKET& pkt);

private:
    CanBusManager canBus;
    PowerSupplyController power;
    CoolingController cooling;
    SensorController sensors;
    
    bool is_system_ok = false;
    volatile int startup_step = 0; // 0 - простой, 1 - ждем пинг FF, 2 - сброс ошибок, 3 - попытка включения, 4 - проверка статуса
    
    void continueStartSystem();

    // Внутренние методы проверки условий
    QTimer *updateTimer;
    float updateFreq;
    void checkInterlocks(float flow, float temp, uint8_t power_status); // TODO: дописать в этот метод проверку давления

    void handleUnexpectedPacket(const CAN_PACKET& pkt);

    // Логирование
    QTimer *dataLogTimer;
    void saveLogToFile(const QString& msg);
    void saveDataLogToFile();

    // Контроль блоков на линии
    bool cdacResponded = false;
    bool cacResponded = false;

    // Контроль таймаутов
    qint64 lastPowerMsgTime = 0;
    qint64 lastSensorMsgTime = 0;
};

#endif // SYSTEM_MANAGER_H