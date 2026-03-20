#ifndef MANAGER_H
#define MANAGER_H

#include "PowerControl.h"
#include "CoolControl.h"
#include "CanBusManager.h"
#include <QObject>

/*
 * Класс SystemManager объединяет все подсистемы установки.
 * Реализует логику безопасности (Interlocks) и предоставляет единый интерфейс для UI.
 */
class SystemManager : public QObject {
    Q_OBJECT
public:
    explicit SystemManager(QObject* parent = nullptr);
    ~SystemManager();

    // --- Логика управления ---
    void startSystem();      // Последовательный запуск (сначала охлаждение)
    void stopSystem();       // Плановый останов
    void emergencyAllStop(); // Немедленный аварийный останов всего железа
    
    // Метод для вызова в цикле таймера (проверка условий безопасности)
    void update(); 

    // --- Проброс команд к питанию ---
    void setCurrent(float amperes);

    // --- Геттеры для интерфейса (получение текущих значений) ---
    float getTemp();
    float getFlow();
    float getCurrent();

    // Состояние системы для индикации в UI
    bool isOk() const { return is_system_ok; }
    uint8_t getStatusFlags();

signals:
    void logMessage(const QString& msg);

private:
    CanBusManager canBus;
    PowerSupplyController power;
    CoolingController cooling;
    
    bool is_system_ok;
    
    // Внутренние методы проверки условий
    void checkInterlocks(float flow, float temp, uint8_t power_status);
};

#endif // SYSTEM_MANAGER_H