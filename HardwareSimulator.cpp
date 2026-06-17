#include "HardwareSimulator.h"
#include "SettingsManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>


SimulatorUI::SimulatorUI(QWidget *parent) : QWidget(parent) {
    setWindowTitle("Симуляция оборудования (HIL)");
    resize(400, 600);
    setAttribute(Qt::WA_DeleteOnClose);

    setAttribute(Qt::WA_QuitOnClose, false);

    auto *mainLayout = new QVBoxLayout(this);

    auto *powBox = new QGroupBox("ВЧ-300 (Питание)");
    auto *powLayout = new QFormLayout(powBox);

    chkPowerOnline = new QCheckBox("Модуль на связи (в сети)");
    chkPowerOnline->setChecked(true);
    powLayout->addRow(chkPowerOnline);
    
    lblPowerState = new QLabel("ВЫКЛ");
    lblPowerState->setStyleSheet("color: red; font-weight: bold;");
    lblTargetCur = new QLabel("0.00 А");
    powVoltSpin = new QDoubleSpinBox();
    powVoltSpin->setRange(0.0, 10.0);
    powVoltSpin->setSuffix(" В (АЦП)");

    chkErrOut1 = new QCheckBox("Защита Вых.1");
    chkErrOut2 = new QCheckBox("Защита Вых.2");
    chkErrTemp = new QCheckBox("Защита Темп.");
    chkErrInv = new QCheckBox("Ошибка Инвертора");
    chkErrPhase = new QCheckBox("Ошибка Фаз");

    powLayout->addRow("Состояние:", lblPowerState);
    powLayout->addRow("Уставка тока (от ПК):", lblTargetCur);
    powLayout->addRow("Эмуляция АЦП:", powVoltSpin);
    powLayout->addRow(chkErrOut1); powLayout->addRow(chkErrOut2);
    powLayout->addRow(chkErrTemp); powLayout->addRow(chkErrInv);
    powLayout->addRow(chkErrPhase);
    mainLayout->addWidget(powBox);

    auto *coolBox = new QGroupBox("Охлаждение (Arduino)");
    auto *coolLayout = new QFormLayout(coolBox);

    chkCoolOnline = new QCheckBox("Модуль на связи (в сети)");
    chkCoolOnline->setChecked(true);
    coolLayout->addRow(chkCoolOnline);

    lblPumpState = new QLabel("ВЫКЛ");
    lblPumpState->setStyleSheet("color: red; font-weight: bold;");
    flowSpin = new QSpinBox();
    flowSpin->setRange(0, 5000);
    flowSpin->setValue(200);
    flowSpin->setSuffix(" (сотни мл/мин)");
    coolLayout->addRow("Состояние Насоса:", lblPumpState);
    coolLayout->addRow("Эмуляция потока:", flowSpin);
    mainLayout->addWidget(coolBox);

    auto *sensBox = new QGroupBox("Датчики (CAC208)");
    auto *sensLayout = new QFormLayout(sensBox);

    chkSensOnline = new QCheckBox("Модуль на связи (в сети)");
    chkSensOnline->setChecked(true);
    sensLayout->addRow(chkSensOnline);

    sensF1Spin = new QDoubleSpinBox(); sensF1Spin->setRange(0, 10);
    sensF2Spin = new QDoubleSpinBox(); sensF2Spin->setRange(0, 10);
    sensHallSpin = new QDoubleSpinBox(); sensHallSpin->setRange(0, 10);
    sensVacSpin = new QDoubleSpinBox(); sensVacSpin->setRange(0, 10);
    sensLayout->addRow("Фарадей 1 (В):", sensF1Spin);
    sensLayout->addRow("Фарадей 2 (В):", sensF2Spin);
    sensLayout->addRow("Холл (В):", sensHallSpin);
    sensLayout->addRow("Вакуум (В):", sensVacSpin);
    mainLayout->addWidget(sensBox);

    auto connectDouble = [&](QDoubleSpinBox* s) { connect(s, &QDoubleSpinBox::valueChanged, this, &SimulatorUI::broadcastValues); };
    auto connectCheck = [&](QCheckBox* c) { connect(c, &QCheckBox::toggled, this, &SimulatorUI::broadcastValues); };
    
    connectDouble(powVoltSpin); connectDouble(sensF1Spin); connectDouble(sensF2Spin);
    connectDouble(sensHallSpin); connectDouble(sensVacSpin);
    connect(flowSpin, &QSpinBox::valueChanged, this, &SimulatorUI::broadcastValues);
    connectCheck(chkErrOut1); connectCheck(chkErrOut2); connectCheck(chkErrTemp);
    connectCheck(chkErrInv); connectCheck(chkErrPhase);

    connectCheck(chkPowerOnline);
    connectCheck(chkCoolOnline);
    connectCheck(chkSensOnline);

    broadcastValues();
}

void SimulatorUI::broadcastValues() {
    uint8_t flags = 0;
    if (lblPowerState->text() == "ВКЛ") flags |= 0x01;
    if (chkErrOut1->isChecked()) flags |= 0x02;
    if (chkErrOut2->isChecked()) flags |= 0x04;
    if (chkErrTemp->isChecked()) flags |= 0x08;
    if (chkErrInv->isChecked()) flags |= 0x10;
    if (chkErrPhase->isChecked()) flags |= 0x20;

    emit powerParamsChanged(powVoltSpin->value(), flags, chkPowerOnline->isChecked());
    emit coolParamsChanged(flowSpin->value(), chkCoolOnline->isChecked());
    emit sensorParamsChanged(sensF1Spin->value(), sensF2Spin->value(), sensHallSpin->value(), sensVacSpin->value(), chkSensOnline->isChecked());
}

void SimulatorUI::onTargetCurrentChanged(float amps) {
    lblTargetCur->setText(QString("%1 А").arg(amps, 0, 'f', 2));
    powVoltSpin->setValue(amps * 8.0 / 300.0);
}

void SimulatorUI::onPumpStateChanged(bool state) {
    lblPumpState->setText(state ? "ВКЛ" : "ВЫКЛ");
    lblPumpState->setStyleSheet(state ? "color: green; font-weight: bold;" : "color: red; font-weight: bold;");
}

void SimulatorUI::onPowerStateChanged(bool state) {
    lblPowerState->setText(state ? "ВКЛ" : "ВЫКЛ");
    lblPowerState->setStyleSheet(state ? "color: green; font-weight: bold;" : "color: red; font-weight: bold;");
    emit broadcastValues();
}

void SimulatorUI::onPowerFlagsChanged() {
    for (QCheckBox* o: {chkErrOut1, chkErrOut2, chkErrTemp, chkErrInv, chkErrPhase}) {
        o->blockSignals(true);
    }

    chkErrOut1->setChecked(false);
    chkErrOut2->setChecked(false);
    chkErrTemp->setChecked(false);
    chkErrInv->setChecked(false);
    chkErrPhase->setChecked(false);

    for (QCheckBox* o: {chkErrOut1, chkErrOut2, chkErrTemp, chkErrInv, chkErrPhase}) {
        o->blockSignals(false);
    }

    emit broadcastValues();
}


VirtualCanBusManager::VirtualCanBusManager(QObject *parent) : CanBusManager(parent) {
    simTimer = new QTimer(this);
    connect(simTimer, &QTimer::timeout, this, &VirtualCanBusManager::onSimTick);
}

VirtualCanBusManager::~VirtualCanBusManager() {
    simTimer->stop();
}

bool VirtualCanBusManager::init(int card, int port) {
    Q_UNUSED(card); Q_UNUSED(port);
    emit logMessage("SIMULATOR: Virtual CAN Bus started.");
    simTimer->start(50);
    return true;
}

void VirtualCanBusManager::close() {
    simTimer->stop();
    emit logMessage("SIMULATOR: Virtual CAN Bus closed.");
}

bool VirtualCanBusManager::sendCommand(uint32_t target_id, uint8_t cmd, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> full = {cmd};
    full.insert(full.end(), payload.begin(), payload.end());
    return sendCommand(target_id, full);
}

bool VirtualCanBusManager::sendCommand(uint32_t target_id, const std::vector<uint8_t>& payload) {
    if (payload.empty()) return false;
    uint8_t cmd = payload[0];

    SettingsManager &sm = SettingsManager::instance();
    uint8_t powId = sm.get("power_deviceId").toInt();
    uint8_t coolId = sm.get("cool_deviceId").toInt();
    uint8_t sensId = sm.get("sensor_deviceId").toInt();

    uint32_t powTarget = (6 << 8) | (powId << 2);
    uint32_t coolTarget = (6 << 8) | (coolId << 2);
    uint32_t sensTarget = (6 << 8) | (sensId << 2);

    CAN_PACKET reply;
    memset(&reply, 0, sizeof(reply));
    reply.rtr = 0;
    
    auto emitReply = [&](uint32_t senderId, std::vector<uint8_t> data) {
        reply.CAN_ID = senderId;
        reply.len = data.size();
        for(size_t i=0; i < data.size(); ++i) reply.data[i] = data[i];
        emit packetReceived(reply);
    };

    if (target_id == powTarget) {
        if (!powerOnline) return true;
        uint32_t recId = (7 << 8) | (powId << 2);
        if (cmd == 0xFF) emitReply(recId, {0xFF});
        else if (cmd == 0x02) simPowerData = true;
        else if (cmd == 0x00) simPowerData = false;
        else if (cmd == 0xF8) emitReply(recId, {0xF8, 0x00, mockPowFlags});
        else if (cmd == 0xF9) {
            uint8_t state = (payload.size() > 1) ? payload[1] : 0;
            if (state == 0x03) emit notifyPowerState(true);
            else if (state == 0x00) emit notifyPowerState(false);
            else if (state == 0x08) emit notifyPowerFlags();
        }
        else if (cmd == 0x80) {
            if (payload.size() >= 4) {
                uint32_t dac = (payload[1] << 16) | (payload[2] << 8) | payload[3];
                double volt = (dac - 0x7FFFFC) / (0xFFFFF8 / 20.0);
                emit notifyTargetCurrent((volt / 8.0) * 300.0);
            }
        }
    } 
    else if (target_id == coolTarget) {
        if (!coolOnline) return true;
        uint32_t recId = (7 << 8) | (coolId << 2);
        if (cmd == 0xFF) emitReply(recId, {0xFF});
        else if (cmd == 0x01) simCoolData = true;
        else if (cmd == 0x00) simCoolData = false;
        else if (cmd == 0x02) {
            uint8_t state = (payload.size() > 1) ? payload[1] : 0;
            emit notifyPumpState(state == 0x01);
            emitReply(recId, {0x02, state});
        }
    }
    else if (target_id == sensTarget) {
        if (!sensorOnline) return true;
        uint32_t recId = (7 << 8) | (sensId << 2);
        if (cmd == 0xFF) emitReply(recId, {0xFF});
        else if (cmd == 0x01) simSensorData = true;
        else if (cmd == 0x00) simSensorData = false;
    }
    return true;
}

uint32_t VirtualCanBusManager::packAdc(float voltage) {
    if (voltage < 0) voltage = 0;
    if (voltage > 10.0f) voltage = 10.0f;
    return static_cast<uint32_t>((voltage / 10.0f) * 0x3FFFFF);
}

void VirtualCanBusManager::onSimTick() {
    SettingsManager &sm = SettingsManager::instance();
    
    if (simPowerData) {
        uint32_t adc = packAdc(mockPowVolt);
        uint32_t recId = (7 << 8) | (sm.get("power_deviceId").toInt() << 2);
        CAN_PACKET p; memset(&p, 0, sizeof(p));
        p.CAN_ID = recId; p.len = 5;
        p.data[0] = 0x02; p.data[1] = 0x00;
        p.data[2] = adc & 0xFF; p.data[3] = (adc >> 8) & 0xFF; p.data[4] = (adc >> 16) & 0xFF;
        emit packetReceived(p);
    }
    
    if (simCoolData) {
        uint32_t recId = (7 << 8) | (sm.get("cool_deviceId").toInt() << 2);
        CAN_PACKET p; memset(&p, 0, sizeof(p));
        p.CAN_ID = recId; p.len = 3;
        p.data[0] = 0x01; p.data[1] = (mockFlow >> 8) & 0xFF; p.data[2] = mockFlow & 0xFF;
        emit packetReceived(p);
    }
    
    if (simSensorData) {
        uint32_t recId = (7 << 8) | (sm.get("sensor_deviceId").toInt() << 2);
        auto sendSens = [&](uint8_t chan, float val) {
            uint32_t adc = packAdc(val);
            CAN_PACKET p; memset(&p, 0, sizeof(p));
            p.CAN_ID = recId; p.len = 5;
            p.data[0] = 0x01; p.data[1] = chan;
            p.data[2] = adc & 0xFF; p.data[3] = (adc >> 8) & 0xFF; p.data[4] = (adc >> 16) & 0xFF;
            emit packetReceived(p);
        };
        sendSens(sm.get("sensor_chanFaraday1").toInt(), mockF1);
        sendSens(sm.get("sensor_chanFaraday2").toInt(), mockF2);
        sendSens(sm.get("sensor_chanHall").toInt(), mockHall);
        sendSens(sm.get("sensor_chanVacuum").toInt(), mockVac);
    }
}

void VirtualCanBusManager::updatePower(float v, uint8_t f, bool isOnline) {
    mockPowVolt = v;
    mockPowFlags = f;
    if (!isOnline) simPowerData = false;
    powerOnline = isOnline;
}
void VirtualCanBusManager::updateCool(int f, bool isOnline) {
    mockFlow = f;
    if (!isOnline) simCoolData = false;
    coolOnline = isOnline;
}
void VirtualCanBusManager::updateSensors(float f1, float f2, float h, float v, bool isOnline) {
    mockF1 = f1;
    mockF2 = f2;
    mockHall = h;
    mockVac = v;
    if (!isOnline) simSensorData = false;
    sensorOnline = isOnline;
}