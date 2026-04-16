#include "Manager.h"
#include <iostream>

SystemManager::SystemManager(QObject* parent) : QObject(parent), is_system_ok(false) {
    // Проброс логов от всех подсистем наверх, в Manager
    connect(&canBus, &CanBusManager::logMessage, this, &SystemManager::logMessage);
    connect(&power, &PowerSupplyController::logMessage, this, &SystemManager::logMessage);
    connect(&cooling, &CoolingController::logMessage, this, &SystemManager::logMessage);

    // Парсер пакетов
    connect(&canBus, &CanBusManager::packetReceived, this, &SystemManager::handleIncomingPacket);

    // Проброс сигнала занятости дальше
    // connect(&power, &PowerSupplyController::deviceBusyStateChanged, this, &SystemManager::busyStateChanged);
    connect(&power, &PowerSupplyController::deviceBusyStateChanged, this, [this]() {
        emit busyStateChanged(this->isBusy());
    });
}

SystemManager::~SystemManager() {
    stopSystem();
    canBus.close();
}

void SystemManager::initHardware() {
    emit logMessage("SystemManager: Objects created.");

    if (!canBus.init()) {
        emit logMessage("SystemManager: Warning! Could not open PCI-7841 driver. Check the device.");
    } else {
        power.setCanInterface(&canBus);
        emit logMessage("SystemManager: CAN-bus is ready.");
        
        // "Прогрев" драйвера (пустой пакет)
        canBus.sendCommand(0x7FF, 0x00, {}); 
    }
}

void SystemManager::startSystem() {
    if (startup_step != 0 || is_system_ok) return;

    if (!canBus.isOpen()) {
        emit logMessage("SystemManager: Warning! CAN is not initialized. Stopping.");
        return;
    }
    
    power.setCanInterface(&canBus);

    emit logMessage("SystemManager: System start in process...");
    startup_step = 1;
    emit busyStateChanged(true);

    // 1. Проверка наличия CDAC на линии
    emit logMessage("SystemManager: Checking CDAC...");
    canBus.sendCommand(power.getTargetId(), 0xFF, {});

    QTimer::singleShot(1000, this, [this]() {
        if (startup_step == 1) {
            emit logMessage("SystemManager: Warning! No response from CDAC. Startup process is stopped");
            stopSystem();
        }
    });
    // Далее после обработки ответного пакета FF startup_step станет == 2 и вызовется continueStartSystem
}

void SystemManager::continueStartSystem() {
    cooling.setPumpState(true);
    cooling.setCoolerState(true);
    
    // Включаем автоматический непрерывный репорт АЦП (0x30 - continuous)
    canBus.sendCommand(power.getTargetId(), 0x02, {0x00, 0x07, 0x30});
    
    // 2. Запускаем сброс защиты (~ 2100 мс)
    startup_step = 2;
    power.resetProtection();

    // Ждем 2200 мс, пока сброс завершится, затем подаем питание
    QTimer::singleShot(2200, this, [this]() {
        // Если запуск прервали кнопкой СТОП
        if (startup_step != 2) {
            emit logMessage("SystemManager: Startup interrupted.");
            return; 
        }

        // 3. Подаем питание (~ 500 мс)
        startup_step = 3;
        power.setPowerState(true);

        // Проверяем финальный статус через 600 мс
        QTimer::singleShot(600, this, [this]() {
            if (startup_step != 3) {
                emit logMessage("SystemManager: Startup interrupted.");
                return; 
            }
            
            // 4. Переходим в режим ожидания подтверждения статуса
            startup_step = 4;
            power.requestRegisters();

            // Таймаут 500мс на подтверждение включения
            QTimer::singleShot(500, this, [this]() {
                if (startup_step != 4) {
                    emit logMessage("SystemManager: Startup interrupted.");
                    return; 
                }

                uint8_t status = power.getStatusFlags();
                if (status & 0x01) { 
                    // Бит питания успешно установился
                    emit logMessage("SystemManager: System started successfully.");
                    is_system_ok = true;
                    startup_step = 0; // Завершили запуск
                    emit busyStateChanged(false);
                }
                else if (status == 0x00) {
                    emit logMessage("SystemManager: Warning! Timeout for startup.");
                    stopSystem();
                }
                else {
                    // Если питание не включилось и висит аппаратная ошибка
                    if (status & 0x02) {
                        emit logMessage("SystemManager: Out protection 1!.");
                    }
                    if (status & 0x04) {
                        emit logMessage("SystemManager: Out protection 2!.");
                    }
                    if (status & 0x08) {
                        emit logMessage("SystemManager: Temperature protection!.");
                    }
                    if (status & 0x10) {
                        emit logMessage("SystemManager: Invertor error!.");
                    }
                    if (status & 0x20) {
                        emit logMessage("SystemManager: Phases error!.");
                    }
                    stopSystem();
                    return;
                }

            });
        });
    });
}

void SystemManager::stopSystem() {
    startup_step = 0;
    if (!is_system_ok && !canBus.isOpen()) return;
    
    emit logMessage("SystemManager: Shutting down...");

    canBus.sendCommand(power.getTargetId(), 0x00, {}); // Выключаем измерения АЦП
    power.setPowerState(false);
    cooling.setPumpState(false);
    cooling.setCoolerState(false);
    
    // canBus.close();
    is_system_ok = false;
    emit busyStateChanged(false);
    emit logMessage("SystemManager: System is off.");
}

void SystemManager::handleIncomingPacket(const CAN_PACKET& pkt) {
    if (!power.isMyReply(pkt.CAN_ID)) return;

    uint8_t cmd = pkt.data[0];

    switch(cmd) {
        case 0x02:
            power.processADCData(pkt);
            break;
        
        case 0xF8:
            power.processRegisterData(pkt);
            break;

        case 0xFF:
            if (startup_step == 1) {
                startup_step = 2;
                emit logMessage("SystemManager: CDAC responded.");
                continueStartSystem();
            }
            break;
        
        default:
            emit logMessage("SystemManager: Received unexpected data");
            emit logMessage(QString("ID: %1 А").arg(QString::number(pkt.CAN_ID, 16)));
            uint64_t data = 0;
            for (int i = pkt.len; i != 0; i--) {
                data = pkt.data[i] << 8 * i;
            }
            emit logMessage(QString("Data: %1 А").arg(QString::number(data, 16)));
            break;
    }
}

void SystemManager::update() {
    if (!canBus.isOpen()) return;

    // Авто-АЦП включен, запрашиваем только регистры
    power.requestRegisters(); 

    checkInterlocks(cooling.getFlowRate(), cooling.getTemperature(), power.getStatusFlags());
}

void SystemManager::checkInterlocks(float flow, float temp, uint8_t power_status) {
    if (startup_step > 0) return; // Во время включения не проверяем

    bool alarm = false;
    
    if (flow < 2.0f) { 
        alarm = true;
        emit logMessage("SystemManager: ALARM! No water flow!");
    }
    if (temp > 75.0f) {
        alarm = true;
        emit logMessage("SystemManager: ALARM! Overheating!");
    }
    // Бит 1-2 (OutProt), Бит 3 (TempProt), Бит 4 (InvertProt), Бит 5 (PhaseProt) => 0x3E (0011 1110)
    if (power_status & 0x3E) {
        alarm = true; 
        emit logMessage("SystemManager: ALARM! Power unit hardware defence!");
    }

    if (alarm) stopSystem();
}

void SystemManager::setCurrent(float amperes) {
    if (is_system_ok) {
        power.setCurrent(amperes);
    } else {
        emit logMessage("SystemManager: Warning! Trying to set current while system is off.");
    }
}

float SystemManager::getTemp() { return cooling.getTemperature(); }
float SystemManager::getFlow() { return cooling.getFlowRate(); }
float SystemManager::getCurrent() { return power.getCurrent(); }
float SystemManager::getAdcVoltage() { return power.getAdcVoltage(); }
uint8_t SystemManager::getStatusFlags() { return power.getStatusFlags(); }
