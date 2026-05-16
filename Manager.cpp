#include <iostream>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QElapsedTimer>
#include "Manager.h"
#include "SettingsManager.h"

SystemManager::SystemManager(QObject* parent) : QObject(parent), updateFreq(10.0f) {
    // Проброс логов от всех подсистем наверх, в Manager
    connect(&canBus, &CanBusManager::logMessage, this, &SystemManager::logMessage);
    connect(&power, &PowerSupplyController::logMessage, this, &SystemManager::logMessage);
    connect(&cooling, &CoolingController::logMessage, this, &SystemManager::logMessage);
    connect(&sensors, &SensorController::logMessage, this, &SystemManager::logMessage);

    // Парсер пакетов
    connect(&canBus, &CanBusManager::packetReceived, this, &SystemManager::handleIncomingPacket);

    // Проброс сигнала занятости дальше
    connect(&power, &PowerSupplyController::deviceBusyStateChanged, this, [this]() {
        emit busyStateChanged(this->isBusy());
    });

    // Логирование
    connect(this, &SystemManager::logMessage, this, &SystemManager::saveLogToFile);
    dataLogTimer = new QTimer(this); // Старт таймера будет при включении системы
    connect(dataLogTimer, &QTimer::timeout, this, &SystemManager::saveDataLogToFile);

    // Таймер на обновление статуса
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &SystemManager::update);
    updateTimer->start(1000.0f / updateFreq);
}

SystemManager::~SystemManager() {
    canBus.sendCommand(power.getTargetId(), 0x00, {}); // Выключаем измерения АЦП
    canBus.sendCommand(sensors.getTargetId(), 0x00, {});
    stopSystem();
    canBus.close();
}

void SystemManager::initHardware() {
    emit logMessage("SystemManager: Objects created.");

    if (!canBus.init()) {
        emit logMessage("SystemManager: Warning! Could not open PCI-7841 driver. Check the device.");
    } else {
        power.setCanInterface(&canBus);
        sensors.setCanInterface(&canBus);
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

    emit logMessage("SystemManager: System start in process...");
    startup_step = 1;
    emit busyStateChanged(true);

    // 0.0 Проверка интерлоков (температура, давление итд)
    if (cooling.getTemperature() > 70.0f) {
        emit logMessage("SystemManager: Start error!. Coolant temperature too high!.");
        stopSystem();
    }

    // 0.1 Получение ответа от arduino о наличии потока

    // TODO

    // 1. Проверка наличия CDAC и CAC на линии
    if (startup_step != 1) return;
    emit logMessage("SystemManager: Checking CDAC20 and CAC208...");
    canBus.sendCommand(power.getTargetId(), 0xFF, {});
    canBus.sendCommand(sensors.getTargetId(), 0xFF, {});

    QTimer::singleShot(500, this, [this]() {
        if (!cdacResponded) {
            emit logMessage("SystemManager: Warning! No response from CDAC20. Startup process is stopped");
            stopSystem();
        } else if (!cacResponded) {
            emit logMessage("SystemManager: Warning! No response from CAC208. Startup process is stopped");
            stopSystem();
        } else {
            startup_step = 2;
            continueStartSystem();
        }
    });
    // Далее после обработки ответного пакета FF вызовется continueStartSystem
}

void SystemManager::continueStartSystem() {
    cooling.setPumpState(true);
    cooling.setCoolerState(true);
    
    // Включаем автоматический непрерывный репорт АЦП (0x30 - continuous)
    canBus.sendCommand(power.getTargetId(), 0x02, {0x00, 0x07, 0x30});
    canBus.sendCommand(sensors.getTargetId(), 0x01, {0x18, 0x19, 0x07, 0x30, 0x00});
    
    // 2. Запускаем сброс защиты (~ 2100 мс)
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
                    dataLogTimer->start(SettingsManager::instance().get("log_intervalMs").toInt());
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
    power.setCurrent(0);
    power.setPowerState(false);
    cooling.setPumpState(false);
    cooling.setCoolerState(false);
    // canBus.sendCommand(power.getTargetId(), 0x00, {}); // Выключаем измерения АЦП
    
    // canBus.close();
    is_system_ok = false;
    emit busyStateChanged(false);
    emit logMessage("SystemManager: System is off.");
}

void SystemManager::handleIncomingPacket(const CAN_PACKET& pkt) {
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    if (power.isMyReply(pkt.CAN_ID)) {
        lastPowerMsgTime = currentTime; // Сброс таймаута
        uint8_t cmd = pkt.data[0];

        if (startup_step == 1 && cmd == 0xFF) {
            emit logMessage("SystemManager: CDAC20 responded.");
            cdacResponded = true;
        } else {
            power.handleMessage(pkt);
        }
    } else if (sensors.isMyReply(pkt.CAN_ID)) {
        lastSensorMsgTime = currentTime; // Сброс таймаута
        if (startup_step == 2 && pkt.data[0] == 0xFF) {
            emit logMessage("SystemManager: CAC208 responded.");
            cacResponded = true;
        }
        else {
            sensors.handleMessage(pkt);
        }
    } else handleUnexpectedPacket(pkt);
}

void SystemManager::handleUnexpectedPacket(const CAN_PACKET& pkt) {
    QString hexData;
    uint64_t data = 0;
    for (int i = 0; i < pkt.len; ++i) {
        hexData += QString("%1 ").arg(pkt.data[i], 2, 16, QChar('0')).toUpper();
    }
    emit logMessage(QString("SystemManager: Received unexpected data. ID: 0x%1 Data(HEX): %2").arg(QString::number(pkt.CAN_ID, 16).toUpper(), hexData.trimmed()));
}

void SystemManager::update() {
    if (!canBus.isOpen()) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Проверка таймаутов (только если система запущена)
    if (is_system_ok) {
        if ((now - lastPowerMsgTime) > 1000) {
            emit logMessage("SystemManager: ERROR! Lost connection with CDAC20 (Timeout)!");
            stopSystem();
        }
        if ((now - lastSensorMsgTime) > 1000) {
            emit logMessage("SystemManager: ERROR! Lost connection with CAC208 (Timeout)!");
            stopSystem();
        }
    }

    // Авто-АЦП включен, запрашиваем только регистры
    power.requestRegisters(); 

    checkInterlocks(cooling.getFlowRate(), cooling.getTemperature(), power.getStatusFlags());
}

void SystemManager::checkInterlocks(float flow, float temp, uint8_t power_status) {
    if (startup_step > 0 || !is_system_ok) return; // Когда выключена или включается не проверяем

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

void SystemManager::saveLogToFile(const QString& formattedMsg) {
    // Создаем папку Logs, если её нет
    QDir dir;
    if (!dir.exists("Logs")) {
        dir.mkdir("Logs");
    }

    // Формируем имя файла DD-MM-YYYY.txt
    QString fileName = QDateTime::currentDateTime().toString("dd-MM-yyyy") + ".txt";
    QFile file("Logs/" + fileName);

    // Открываем в режиме Append (дозапись)
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        // out.setEncoding(QStringConverter::Encoding::Utf8); // Для корректной поддержки кириллицы
        out << formattedMsg << "\n";
        file.close();
    }
}

void SystemManager::saveDataLogToFile() {
    QJsonObject root;
    // Используем миллисекунды для точной привязки к оси QDateTimeAxis в графиках
    root["time"] = static_cast<double>(QDateTime::currentMSecsSinceEpoch());

    QJsonObject power;
    power["current"] = getCurrent();

    QJsonObject cool;
    cool["temp"] = getTemp();
    cool["flow"] = getFlow();

    QJsonObject sensors;
    sensors["hall"] = getHall();
    sensors["ioncurrent"] = getFaraday();
    sensors["pressure"] = SensorController::getPressFromVolt(getVacuum());

    root["powercontroller"] = power;
    root["coolcontroller"] = cool;
    root["sensorcontroller"] = sensors;

    QJsonDocument doc(root);
    QString fileName = "Data-" + QDateTime::currentDateTime().toString("dd-MM-yyyy") + ".jsonl";
    
    QDir dir;
    if (!dir.exists("Logs")) dir.mkdir("Logs");
    QFile file("Logs/" + fileName);
    
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << doc.toJson(QJsonDocument::Compact) << "\n";
        file.close();
    }
}