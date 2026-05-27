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
        updateFreq(10.0f),
        m_eventLogger(std::move(eventLogger)),
        m_telemetryLogger(std::move(telemetryLogger))
{
    connect(&canBus, &CanBusManager::logMessage, this, &SystemManager::logMessage);
    connect(&power, &PowerSupplyController::logMessage, this, &SystemManager::logMessage);
    connect(&cooling, &CoolingController::logMessage, this, &SystemManager::logMessage);
    connect(&sensors, &SensorController::logMessage, this, &SystemManager::logMessage);

    connect(&canBus, &CanBusManager::packetReceived, this, &SystemManager::handleIncomingPacket);

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
        if (!cdacResponded) {
            emit logMessage("SystemManager: Warning! No response from CDAC20. Startup process is stopped");
            stopSystem();
        } else if (!cacResponded) {
            emit logMessage("SystemManager: Warning! No response from CAC208. Startup process is stopped");
            stopSystem();
        } else if (!arduinoResponded) {
            emit logMessage("SystemManager: Warning! No response from Arduino. Startup process is stopped");
            stopSystem();
        } else {
            startup_step = 2;
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

            if (cooling.getTemperature() > 70.0f) {
                emit logMessage("SystemManager: Warning! Temperature too high! Startup process is stopped");
                stopSystem();
            } else {
                cooling.setPumpState(true); // Ардуино отправит сообщение о наличии/отсутствии потока
                cooling.setCoolerState(true);
                QTimer::singleShot(1000, this, [this]() {
                    if (!cooling.getPumpState()) { // Если не поднялся поток то будет false
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
                        qint64 now = QDateTime::currentMSecsSinceEpoch();
                        lastPowerMsgTime = now;
                        lastSensorMsgTime = now;
                        lastCoolMsgTime = now;

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
    power.setCurrent(0);
    power.setPowerState(false);
    cooling.setPumpState(false);
    cooling.setCoolerState(false);
    
    cdacResponded = false;
    cacResponded = false;
    arduinoResponded = false;
    is_running = false;
    emit busyStateChanged(false);
    emit logMessage("SystemManager: System is off.");
}

void SystemManager::handleIncomingPacket(const CAN_PACKET& pkt) {
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    if (power.isMyReply(pkt.CAN_ID)) {
        lastPowerMsgTime = currentTime;
        uint8_t cmd = pkt.data[0];

        if (startup_step == 1 && cmd == 0xFF) {
            emit logMessage("SystemManager: CDAC20 responded.");
            cdacResponded = true;
        } else {
            power.handleMessage(pkt);
        }
    } else if (sensors.isMyReply(pkt.CAN_ID)) {
        lastSensorMsgTime = currentTime;
        if (startup_step == 1 && pkt.data[0] == 0xFF) {
            emit logMessage("SystemManager: CAC208 responded.");
            cacResponded = true;
        }
        else {
            sensors.handleMessage(pkt);
        }
    } else if (cooling.isMyReply(pkt.CAN_ID)) {
        if (startup_step == 1 && pkt.data[0] == 0xFF) {
            emit logMessage("SystemManager: Arduino responded.");
            arduinoResponded = true;
        }
        lastCoolMsgTime = currentTime;
        cooling.handleMessage(pkt);
    }
    else handleUnexpectedPacket(pkt);
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

    if (is_running) {
        if ((now - lastPowerMsgTime) > 1000) {
            emit logMessage("SystemManager: ERROR! Lost connection with CDAC20 (power), timeout!");
            stopSystem();
        }
        if ((now - lastSensorMsgTime) > 1000) {
            emit logMessage("SystemManager: ERROR! Lost connection with CAC208 (sensors), timeout!");
            stopSystem();
        }
        if ((now - lastCoolMsgTime) > 1000) {
            emit logMessage("SystemManager: ERROR! Lost connection with Arduino (cooling), Timeout!");
            stopSystem();
        }
    }

    power.requestRegisters(); 

    checkInterlocks(cooling.getFlowRate(), cooling.getTemperature(), power.getStatusFlags());
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
    
    if (m_eventLogger) {
        m_eventLogger->log("Logs", fileName, formattedMsg);
    }
}

void SystemManager::onDataLogTimeout() {
    QString fileName = "Data-" + QDateTime::currentDateTime().toString("dd-MM-yyyy");
    
    QVariantMap data;
    data["timestamp"]   = QDateTime::currentMSecsSinceEpoch();
    data["current"]     = getCurrent();
    data["temp"]        = getTemp();
    data["flow"]        = getFlow();
    data["hall"]        = getHall();
    data["ioncurrent"]  = getFaraday();
    data["pressure"]    = sensors.getPressFromVolt(getVacuum()); 
    if (m_telemetryLogger) {
        QThreadPool::globalInstance()->start(new LogTask(m_telemetryLogger.get(), "Logs", fileName, data));
    }
}
