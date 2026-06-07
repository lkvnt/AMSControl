#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "PowerControl.h"
#include "CoolControl.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>

SettingsDialog::SettingsDialog(const QString& tabName, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Настройки: " + tabName);
    resize(350, 200);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    SettingsManager &sm = SettingsManager::instance();

    if (tabName == "Главная") {
        auto *tLayout = new QHBoxLayout();
        auto *freqSpin = new QSpinBox();
        freqSpin->setRange(1, 100);
        int oldFreq = sm.get("update_frequency").toInt();
        freqSpin->setValue(oldFreq);
        auto *pollSpin = new QSpinBox();
        pollSpin->setRange(1, 1000);
        int oldPoll = sm.get("can_bus_poll_timer").toInt();
        pollSpin->setValue(oldPoll);
        tLayout->addWidget(new QLabel("UI update frequency: "));
        tLayout->addWidget(freqSpin);
        tLayout->addWidget(new QLabel("CAN bus poll timer: "));
        tLayout->addWidget(pollSpin);
        form->addRow(tLayout);

        auto *pLayout = new QHBoxLayout();
        auto *powerOnBtn = new QPushButton("Turn on VCH-300");
        connect(powerOnBtn, &QPushButton::clicked, this, &SettingsDialog::reqPowerOn);
        auto *powerOffBtn = new QPushButton("Turn off VCH-300");
        connect(powerOffBtn, &QPushButton::clicked, this, &SettingsDialog::reqPowerOff);
        auto *resetProt = new QPushButton("Reset prot. VCH-300");
        connect(resetProt, &QPushButton::clicked, this, &SettingsDialog::reqResetProt);
        pLayout->addWidget(powerOnBtn);
        pLayout->addWidget(powerOffBtn);
        pLayout->addWidget(resetProt);
        form->addRow(pLayout);

        auto *currLayout = new QHBoxLayout();
        auto *currSpin = new QSpinBox();
        currSpin->setRange(0, 300);
        currSpin->setValue(0);
        auto *currSetBtn = new QPushButton("Set current");
        connect(currSetBtn, &QPushButton::clicked, this, [this, currSpin]() {
            emit reqSetCurrent(static_cast<float>(currSpin->value()));
        });
        currLayout->addWidget(new QLabel("VCH-300 current (A): "));
        currLayout->addWidget(currSpin);
        currLayout->addWidget(currSetBtn);
        form->addRow(currLayout);

        auto *cLayout = new QHBoxLayout();
        auto *coolOnBtn = new QPushButton("Turn on cooling");
        connect(coolOnBtn, &QPushButton::clicked, this, &SettingsDialog::reqCoolingOn);
        auto *coolOffBtn = new QPushButton("Turn off cooling");
        connect(coolOffBtn, &QPushButton::clicked, this, &SettingsDialog::reqCoolingOff);
        cLayout->addWidget(coolOnBtn);
        cLayout->addWidget(coolOffBtn);
        form->addRow(cLayout);

        auto *sLayout = new QHBoxLayout();
        auto *sensorDataBtn = new QPushButton("Request sensor data");
        connect(sensorDataBtn, &QPushButton::clicked, this, &SettingsDialog::reqSensorData);
        sLayout->addWidget(sensorDataBtn);
        form->addRow(sLayout);
        
        connect(this, &QDialog::accepted, [=, &sm]() {
            if ((freqSpin->value() != oldFreq) || (pollSpin->value() != oldPoll)) {
                sm.set("update_frequency", freqSpin->value());
                sm.set("can_bus_poll_timer", pollSpin->value());
                QMessageBox::information(nullptr, "Требуется перезагрузка", 
                    "Новые параметры успешно сохранены в settings.json.\n\n"
                    "Пожалуйста, перезапустите программу, чтобы изменения вступили в силу.");
            }
        });
    }

    else if (tabName == "Питание") {
        auto *idSpin = new QSpinBox();
        idSpin->setDisplayIntegerBase(16);
        idSpin->setPrefix("0x");
        idSpin->setRange(0, 255);
        int oldId = sm.get("power_deviceId").toInt();
        idSpin->setValue(oldId);
        
        form->addRow("Device ID (CDAC20):", idSpin);
        
        connect(this, &QDialog::accepted, [=, &sm]() {
            if (idSpin->value() != oldId) {
                sm.set("power_deviceId", idSpin->value());
                QMessageBox::information(nullptr, "Требуется перезагрузка", 
                    "Новые параметры успешно сохранены в settings.json.\n\n"
                    "Пожалуйста, перезапустите программу, чтобы изменения вступили в силу.");
            }
        });
    } 

    else if (tabName == "Охлаждение") {
        auto *idSpin = new QSpinBox();
        idSpin->setDisplayIntegerBase(16);
        idSpin->setPrefix("0x");
        idSpin->setRange(0, 255);
        int oldId = sm.get("cool_deviceId").toInt();
        idSpin->setValue(oldId);

        auto *tSpin = new QDoubleSpinBox(); 
        tSpin->setRange(-50.0, 200.0);
        tSpin->setValue(sm.get("cool_mockTemp").toDouble());

        form->addRow("Device ID (Arduino)", idSpin);
        form->addRow("Заглушка Температура (°C):", tSpin);
        
        connect(this, &QDialog::accepted, [=, &sm]() {
            sm.set("cool_mockTemp", tSpin->value());
            if (idSpin->value() != oldId) {
                sm.set("sensor_deviceId", idSpin->value());
                QMessageBox::information(nullptr, "Требуется перезагрузка", 
                    "Новые параметры успешно сохранены в settings.json.\n\n"
                    "Пожалуйста, перезапустите программу, чтобы изменения вступили в силу.");
            }
        });
    }

    else if (tabName == "Измерения") {
        auto *idSpin = new QSpinBox();
        idSpin->setDisplayIntegerBase(16);
        idSpin->setPrefix("0x");
        idSpin->setRange(0, 255);
        int oldId = sm.get("sensor_deviceId").toInt();
        idSpin->setValue(oldId);

        auto *chFaraday1 = new QSpinBox();
        chFaraday1->setDisplayIntegerBase(16);
        chFaraday1->setPrefix("0x");
        chFaraday1->setRange(0, 255);
        chFaraday1->setValue(sm.get("sensor_chanFaraday1").toInt());

        auto *chFaraday2 = new QSpinBox();
        chFaraday2->setDisplayIntegerBase(16);
        chFaraday2->setPrefix("0x");
        chFaraday2->setRange(0, 255);
        chFaraday2->setValue(sm.get("sensor_chanFaraday2").toInt());

        auto *chHall = new QSpinBox();
        chHall->setDisplayIntegerBase(16);
        chHall->setPrefix("0x");
        chHall->setRange(0, 255);
        chHall->setValue(sm.get("sensor_chanHall").toInt());

        auto *chVacuum = new QSpinBox();
        chVacuum->setDisplayIntegerBase(16);
        chVacuum->setPrefix("0x");
        chVacuum->setRange(0, 255);
        chVacuum->setValue(sm.get("sensor_chanVacuum").toInt());

        form->addRow("Device ID (CAC208):", idSpin);
        form->addRow("Канал: Цилиндр Фарадея 1:", chFaraday1);
        form->addRow("Канал: Цилиндр Фарадея 2:", chFaraday2);
        form->addRow("Канал: Датчик Холла:", chHall);
        form->addRow("Канал: Давление (ВМБ-14):", chVacuum);

        connect(this, &QDialog::accepted, [=, &sm]() {
            sm.set("sensor_chanFaraday1", chFaraday1->value());
            sm.set("sensor_chanFaraday2", chFaraday2->value());
            sm.set("sensor_chanHall", chHall->value());
            sm.set("sensor_chanVacuum", chVacuum->value());
            if (idSpin->value() != oldId) {
                sm.set("sensor_deviceId", idSpin->value());
                QMessageBox::information(nullptr, "Требуется перезагрузка", 
                    "Новые параметры успешно сохранены в settings.json.\n\n"
                    "Пожалуйста, перезапустите программу, чтобы изменения вступили в силу.");
            }
        });
    }

    else if (tabName == "Логи") {
        auto *logSpin = new QSpinBox();
        logSpin->setRange(100, 60000);
        logSpin->setSuffix(" мс");
        logSpin->setSingleStep(100);
        int oldLog = sm.get("log_intervalMs").toInt();
        logSpin->setValue(oldLog);

        form->addRow("Интервал записи данных:", logSpin);

        connect(this, &QDialog::accepted, [=, &sm]() {
            if (logSpin->value() != oldLog) {
                sm.set("log_intervalMs", logSpin->value());
                QMessageBox::information(nullptr, "Требуется перезагрузка", 
                    "Новые параметры успешно сохранены в settings.json.\n\n"
                    "Пожалуйста, перезапустите программу, чтобы изменения вступили в силу.");
            }
        });
    }

    layout->addLayout(form);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);

    connect(this, &QDialog::accepted, this, []() {
        SettingsManager::instance().save();
    });
}