#ifndef MANAGER_H
#define MANAGER_H

#include "PowerControl.h"
#include "CoolControl.h"

/**
 * @brief Класс SystemManager объединяет все подсистемы установки.
 * Реализует логику безопасности (Interlocks) и предоставляет единый интерфейс для UI.
 */
class SystemManager {
public:
    SystemManager();

    // --- Логика управления ---
    void startSystem();      // Последовательный запуск (сначала охлаждение)
    void stopSystem();       // Плановый останов
    void emergencyAllStop(); // Немедленный аварийный останов всего железа
    
    // Метод для вызова в цикле таймера (проверка условий безопасности)
    void update(); 

    // --- Проброс команд к питанию ---
    void setCurrent(float amperes);

    // --- Геттеры для интерфейса (получение текущих значений) ---
    float getTemp() { return cooling.getTemperature(); }
    float getFlow() { return cooling.getFlowRate(); }
    float getCurrent() { return power.getCurrent(); }

    // Состояние системы для индикации в UI
    bool isOk() const { return is_system_ok; }

private:
    PowerSupplyController power;
    CoolingController cooling;
    
    bool is_system_ok;
    
    // Внутренние методы проверки условий
    void checkInterlocks(float flow, float temp);
};

#endif // SYSTEM_MANAGER_H