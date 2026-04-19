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
    void startSystem();      // Последовательный запуск (сначала охлаждение)
    void stopSystem();       // Плановый останов
    void emergencyAllStop(); // Немедленный аварийный останов всего железа
    
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
    bool isOk() const { return is_system_ok; }
    bool isBusy() const { return power.isDeviceBusy() || startup_step > 0; }
    uint8_t getStatusFlags() const { return power.getStatusFlags(); }

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
    
    bool is_system_ok;
    int startup_step; // 0 - простой, 1 - ждем пинг FF, 2 - сброс ошибок, 3 - попытка включения, 4 - проверка статуса
    
    void continueStartSystem();

    // Внутренние методы проверки условий
    void checkInterlocks(float flow, float temp, uint8_t power_status);
};

#endif // SYSTEM_MANAGER_H