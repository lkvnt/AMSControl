#include <iostream>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QElapsedTimer>
#include <QThreadPool>
#include "Manager.h"
#include "SettingsManager.h"

SystemManager::SystemManager(std::unique_ptr<Logger> eventLogger,
                             std::unique_ptr<Logger> telemetryLogger,
                             QObject* parent)
    :   QObject(parent),
        updateFreq(10.0f)
{
    logWorker = new LogWorker(std::move(eventLogger), std::move(telemetryLogger));
    logWorker->moveToThread(&logThread);
    connect(&logThread, &QThread::finished, logWorker, &QObject::deleteLater);
    connect(this, &SystemManager::requestEventLog, logWorker, &LogWorker::onEventLogRequested);
    connect(this, &SystemManager::requestTelemetryLog, logWorker, &LogWorker::onTelemetryLogRequested);
    logThread.start();

    connect(&canBus, &CanBusManager::logMessage, this, &SystemManager::logMessage);
    connect(&power, &PowerSupplyController::logMessage, this, &SystemManager::logMessage);
    connect(&cooling, &CoolingController::logMessage, this, &SystemManager::logMessage);
    connect(&sensors, &SensorController::logMessage, this, &SystemManager::logMessage);

    connect(this, &SystemManager::stopSystemSignal, &sensors, &SensorController::onSystemStop);
    connect(this, &SystemManager::stopSystemSignal, &power, &PowerSupplyController::onSystemStop);
    connect(this, &SystemManager::stopSystemSignal, &cooling, &CoolingController::onSystemStop);

    connect(&power, &PowerSupplyController::deviceBusyStateChanged, this, [this]() {
        emit busyStateChanged(this->isBusy());
    });

    connect(this, &SystemManager::logMessage, this, &SystemManager::onLogMessageReceived);

    dataLogTimer = new QTimer(this); // Старт таймера будет при включении системы
    connect(dataLogTimer, &QTimer::timeout, this, &SystemManager::onDataLogTimeout);

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &SystemManager::update);
    updateTimer->start(1000.0f / updateFreq);
}

SystemManager::~SystemManager() {
    logThread.quit();
    logThread.wait();

    power.stopDataFlow();
    sensors.stopDataFlow();
    cooling.stopDataFlow();
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
        cooling.setCanInterface(&canBus);
        emit logMessage("SystemManager: CAN-bus is ready."); 
    }
}

void SystemManager::startSystem() {
    if (startup_step != 0 || is_running) return;

    if (!canBus.isOpen()) {
        emit logMessage("SystemManager: Warning! CAN is not initialized. Stopping.");
        return;
    }

    emit logMessage("SystemManager: System start in process...");
    startup_step = 1;
    emit busyStateChanged(true);

    if (startup_step != 1) return;
    emit logMessage("SystemManager: Checking CDAC20, CAC208 and Arduino...");
    power.requestConnection();
    sensors.requestConnection();
    cooling.requestConnection();

    QTimer::singleShot(500, this, [this]() {
        if (!power.isResponded()) {
            emit logMessage("SystemManager: Warning! No response from CDAC20. Startup process is stopped");
            stopSystem();
        } else if (!sensors.isResponded()) {
            emit logMessage("SystemManager: Warning! No response from CAC208. Startup process is stopped");
            stopSystem();
        } else if (!cooling.isResponded()) {
            emit logMessage("SystemManager: Warning! No response from Arduino. Startup process is stopped");
            stopSystem();
        } else {
            startup_step = 2;
            emit logMessage("SystemManager: All blocks responded.");
            continueStartSystem(startup_step);
        }
    });
}

void SystemManager::continueStartSystem(int step) {
    if (step == 2){
        power.requestDataFlow();
        sensors.requestDataFlow();
        cooling.requestDataFlow();

        QTimer::singleShot(500, this, [this]() {
            if (startup_step != 2) {
                emit logMessage("SystemManager: Startup interrupted.");
                return; 
            }

            if (getTemp() > 70.0f) {
                emit logMessage("SystemManager: Warning! Temperature too high! Startup process is stopped");
                stopSystem();
            } else {
                cooling.setState(true); // Ардуино отправит сообщение о наличии/отсутствии потока
                QTimer::singleShot(1000, this, [this]() {
                    if (!cooling.getState()) { // Если не поднялся поток то будет false
                        emit logMessage("SystemManager: Warning! Pump error! Startup process is stopped");
                        stopSystem();
                    } else {
                        startup_step = 3;
                        continueStartSystem(startup_step);
                    }
                });
            }
        });
    }
    if (step == 3) {
        power.resetProtection();
        QTimer::singleShot(500, this, [this]() {
            if (startup_step != 3) {
                emit logMessage("SystemManager: Startup interrupted.");
                return; 
            }

            startup_step = 4;
            power.setPowerState(true);

            QTimer::singleShot(500, this, [this]() {
                if (startup_step != 4) {
                    emit logMessage("SystemManager: Startup interrupted.");
                    return; 
                }

                startup_step = 5;
                power.requestRegisters();

                QTimer::singleShot(500, this, [this]() {
                    if (startup_step != 5) {
                        emit logMessage("SystemManager: Startup interrupted.");
                        return; 
                    }

                    uint8_t status = power.getStatusFlags();
                    if (status & 0x01) { 
                        emit logMessage("SystemManager: System started successfully.");

                        is_running = true;
                        dataLogTimer->start(SettingsManager::instance().get("log_intervalMs").toInt());
                        startup_step = 0;
                        emit busyStateChanged(false);
                    }
                    else if (status == 0x00) {
                        emit logMessage("SystemManager: Warning! Timeout for startup.");
                        stopSystem();
                    }
                    else {
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
}

void SystemManager::stopSystem() {
    startup_step = 0;
    if (!is_running && !canBus.isOpen()) return;
    
    emit logMessage("SystemManager: Shutting down...");

    emit stopSystemSignal();

    is_running = false;
    emit busyStateChanged(false);
    emit logMessage("SystemManager: System is off.");
}

void SystemManager::update() {
    if (!canBus.isOpen()) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (is_running) {
        if ((now - power.getLastMsgTime()) > 1000) {
            emit logMessage("SystemManager: ERROR! Lost connection with CDAC20 (power), timeout!");
            stopSystem();
        }
        if ((now - sensors.getLastMsgTime()) > 1000) {
            emit logMessage("SystemManager: ERROR! Lost connection with CAC208 (sensors), timeout!");
            stopSystem();
        }
        if ((now - cooling.getLastMsgTime()) > 1000) {
            emit logMessage("SystemManager: ERROR! Lost connection with Arduino (cooling), Timeout!");
            stopSystem();
        }
    }

    power.requestRegisters(); 

    checkInterlocks(getFlow(), getTemp(), getStatusFlags());
}

void SystemManager::checkInterlocks(float flow, float temp, uint8_t power_status) {
    if (startup_step > 0 || !is_running) return;

    bool alarm = false;
    
    if (flow < 2.0f) { 
        alarm = true;
        emit logMessage("SystemManager: ALARM! No water flow!");
    }
    if (temp > 75.0f) {
        alarm = true;
        emit logMessage("SystemManager: ALARM! Overheating!");
    }
    if (power_status & power.REGISTER_HAS_ERROR) {
        alarm = true; 
        emit logMessage("SystemManager: ALARM! Power unit hardware defence!");
    }

    if (alarm) stopSystem();
}

void SystemManager::setCurrent(float amperes, bool manual) {
    if (is_running || manual) {
        power.setCurrent(amperes);
    } else {
        emit logMessage("SystemManager: Warning! Trying to set current while system is off.");
    }
}

void SystemManager::onLogMessageReceived(const QString& formattedMsg) {
    QString fileName = "Log-" + QDateTime::currentDateTime().toString("dd-MM-yyyy");
    
    emit requestEventLog("Logs", fileName, formattedMsg);
}

void SystemManager::onDataLogTimeout() {
    QString fileName = "Data-" + QDateTime::currentDateTime().toString("dd-MM-yyyy");
    
    QVariantMap data;
    auto pressure = sensors.getPressFromVolt(getVacuum());
    data["timestamp"]   = QDateTime::currentMSecsSinceEpoch();
    data["current"]     = getCurrent();
    data["temp"]        = getTemp();
    data["flow"]        = getFlow();
    data["hall"]        = getHall();
    data["ioncurrent"]  = getFaraday();
    if (pressure.has_value()) {
        data["pressure"] = pressure.value();
    }
     
    emit requestTelemetryLog("Logs", fileName, data);
}
