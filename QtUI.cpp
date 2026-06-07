#include <QHBoxLayout>
#include <QStatusBar>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>
#include <QGroupBox>
#include "Manager.h"
#include "DataViewerWindow.h"
#include "QtUI.h"
#include "SettingsDialog.h"
#include "SettingsManager.h"

MainWindow::MainWindow(SystemManager *manager, QWidget *parent)
    : QMainWindow(parent), systemManager(manager), time_axis(0.0f)
{
    setupUI();
    resize(900, 700);

    elapsedTimer.start();

    connect(systemManager, &SystemManager::logMessage, this, &MainWindow::onLogMessage);

    connect(systemManager, &SystemManager::busyStateChanged, this, &MainWindow::onBusyStateChanged);

    updateTimer = new QTimer(this);
    updateFreq = SettingsManager::instance().get("update_frequency").toInt();
    if (updateFreq <= 0) updateFreq = 10;
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::onTimerTick);
    updateTimer->start(1000.0 / updateFreq);

    onBusyStateChanged(false);
    QTimer::singleShot(200, this, [this]() {
        systemManager->initHardware();
    });
}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    auto *ctrlLayout = new QHBoxLayout();

    QString baseButtonStyle = 
        "QPushButton {"
        "  border: 1px solid #555;"
        "  border-radius: 5px;"
        "  padding: 5px;"
        "  background-color: #444;"
        "  color: white;"
        "}"
        "QPushButton:hover:enabled {"
        "  background-color: #5a5a5a;"
        "  border: 1px solid #888;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #1a1a1a;"
        "  padding-left: 7px; padding-top: 7px;"
        "  border: 1px solid #333;"
        "}"
        "QPushButton:disabled {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #333, stop:0.5 #444, stop:1 #333);"
        "  color: #777;"
        "  border: 1px dashed #555;"
        "}";


    startBtn = new QPushButton("ЗАПУСК СИСТЕМЫ");
    startBtn->setStyleSheet(baseButtonStyle + 
        "QPushButton:enabled { background-color: #2e8b57; font-weight: bold; height: 35px; } "
        "QPushButton:hover:enabled { background-color: #3cb371; border: 1px solid #fff; }");
    
    stopBtn = new QPushButton("СТОП СИСТЕМЫ");
    stopBtn->setStyleSheet(baseButtonStyle + 
        "QPushButton:enabled { background-color: #d2691e; font-weight: bold; height: 35px; } "
        "QPushButton:hover:enabled { background-color: #e67e22; border: 1px solid #fff; }");

    mainSettingsBtn = new QPushButton("⚙");
    mainSettingsBtn->setFixedSize(30, 30);
    mainSettingsBtn->setStyleSheet(baseButtonStyle);
    connect(mainSettingsBtn, &QPushButton::clicked, [this]() {
        SettingsDialog dlg("Главная", this);

        connect(&dlg, &SettingsDialog::reqPowerOn, systemManager, &SystemManager::manualPowerOn);
        connect(&dlg, &SettingsDialog::reqPowerOff, systemManager, &SystemManager::manualPowerOff);
        connect(&dlg, &SettingsDialog::reqResetProt, systemManager, &SystemManager::manualResetProt);
        connect(&dlg, &SettingsDialog::reqSetCurrent, systemManager, &SystemManager::manualSetCurrent);
        connect(&dlg, &SettingsDialog::reqCoolingOn, systemManager, &SystemManager::manualCoolingOn);
        connect(&dlg, &SettingsDialog::reqCoolingOff, systemManager, &SystemManager::manualCoolingOff);
        connect(&dlg, &SettingsDialog::reqSensorData, systemManager, &SystemManager::manualRequestSensorData);

        dlg.exec();
    });
    
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::handleStart);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::handleStop);

    ctrlLayout->addWidget(startBtn);
    ctrlLayout->addWidget(stopBtn);
    ctrlLayout->addWidget(mainSettingsBtn);
    mainLayout->addLayout(ctrlLayout);


    QGroupBox *globalInfoBox = new QGroupBox("Основная информация системы");
    globalInfoBox->setStyleSheet("QGroupBox { font-weight: bold; border: 1px solid #555; margin-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");
    auto *gLayout = new QHBoxLayout(globalInfoBox);
    
    globalCurrent = new QLabel("Ток: -- А");
    globalCurrent->setStyleSheet("font-size: 18px; font-weight: bold; color: #4CAF50;");
    
    globalTemp = new QLabel("Темп: -- °C");
    globalTemp->setStyleSheet("font-size: 18px; font-weight: bold; color: #2196F3;");
    
    globalPowerLed = new QLabel("ПИТАНИЕ");
    globalPowerLed->setAlignment(Qt::AlignCenter);
    
    globalErrorLed = new QLabel("ОШИБКА");
    globalErrorLed->setAlignment(Qt::AlignCenter);
    
    gLayout->addWidget(globalCurrent);
    gLayout->addWidget(globalTemp);
    gLayout->addStretch();
    gLayout->addWidget(globalPowerLed);
    gLayout->addWidget(globalErrorLed);
    
    mainLayout->addWidget(globalInfoBox);


    auto *tabs = new QTabWidget();

    auto *powerTab = new QWidget();
    auto *pLayout = new QVBoxLayout(powerTab);
    
    auto *currLayout = new QHBoxLayout();
    currentSpinBox = new QDoubleSpinBox();
    currentSpinBox->setRange(0, 300);
    setBtn = new QPushButton("Установить ток");
    setBtn->setStyleSheet(baseButtonStyle);
    connect(setBtn, &QPushButton::clicked, this, &MainWindow::handleSetCurrent);
    powSettingsBtn = new QPushButton("⚙");
    powSettingsBtn->setFixedSize(30, 30);
    powSettingsBtn->setStyleSheet(baseButtonStyle);
    connect(powSettingsBtn, &QPushButton::clicked, [this]() {
        SettingsDialog dlg("Питание", this);
        dlg.exec();
    });
    currLayout->addWidget(new QLabel("Целевой ток (А):"));
    currLayout->addWidget(currentSpinBox);
    currLayout->addWidget(setBtn);
    currLayout->addWidget(powSettingsBtn);
    pLayout->addLayout(currLayout);

    auto *statusLayout = new QHBoxLayout();
    QString ledStyle = "border-radius: 5px; min-width: 120px; min-height: 25px; background-color: gray; color: white; font-weight: bold; qproperty-alignment: 'AlignCenter';";
    
    powerLed = new QLabel("ПИТАНИЕ"); powerLed->setStyleSheet(ledStyle);
    outProt1Led = new QLabel("ЗАЩИТА ВЫХ.1"); outProt1Led->setStyleSheet(ledStyle);
    outProt2Led = new QLabel("ЗАЩИТА ВЫХ.2"); outProt2Led->setStyleSheet(ledStyle);
    tempProtLed = new QLabel("ЗАЩИТА ТЕМП."); tempProtLed->setStyleSheet(ledStyle);
    phaseErrLed = new QLabel("ОШИБКА ФАЗ"); phaseErrLed->setStyleSheet(ledStyle);
    invErrLed = new QLabel("ОШИБКА ИНВ"); invErrLed->setStyleSheet(ledStyle);
    
    statusLayout->addWidget(powerLed);
    statusLayout->addWidget(outProt1Led);
    statusLayout->addWidget(outProt2Led);
    statusLayout->addWidget(tempProtLed);
    statusLayout->addWidget(phaseErrLed);
    statusLayout->addWidget(invErrLed);
    pLayout->addLayout(statusLayout);

    currentSeries = new QLineSeries();
    currentSeries->setUseOpenGL(true);
    currentChart = new QChart();
    currentChart->addSeries(currentSeries);
    currentChart->createDefaultAxes();
    currentChart->axes(Qt::Vertical).first()->setRange(0, 350);
    currentChart->setTitle("Мониторинг тока (А)");
    currentChart->legend()->hide();

    auto *chartView = new QChartView(currentChart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(250);
    pLayout->addWidget(chartView);

    auto *valLayout = new QHBoxLayout();
    currentValLabel = new QLabel("Текущий ток: -- А");
    currentValLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #4CAF50;");
    adcVoltLabel = new QLabel("Напряжение АЦП: -- В");
    adcVoltLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #2196F3;");
    valLayout->addWidget(currentValLabel);
    valLayout->addWidget(adcVoltLabel);
    pLayout->addLayout(valLayout);


    auto *coolingTab = new QWidget();
    auto *cLayout = new QVBoxLayout(coolingTab);

    auto *cStatusLayout = new QHBoxLayout();
    QString cLedStyle = "border-radius: 5px; min-width: 120px; min-height: 25px; background-color: gray; color: white; font-weight: bold; qproperty-alignment: 'AlignCenter';";
    pumpLed = new QLabel("НАСОС"); pumpLed->setStyleSheet(ledStyle);
    radiatorLed = new QLabel("РАДИАТОР"); radiatorLed->setStyleSheet(ledStyle);
    cStatusLayout->addWidget(pumpLed);
    cStatusLayout->addWidget(radiatorLed);

    coolSettingsBtn = new QPushButton("⚙");
    coolSettingsBtn->setFixedSize(30, 30);
    coolSettingsBtn->setStyleSheet(baseButtonStyle);
    connect(coolSettingsBtn, &QPushButton::clicked, [this]() {
        SettingsDialog dlg("Охлаждение", this);
        dlg.exec();
    });
    cStatusLayout->addWidget(coolSettingsBtn);
    cLayout->addLayout(cStatusLayout);

    tempLabel = new QLabel("Температура: -- °C");
    tempLabel->setStyleSheet("font-size: 16px; margin: 5px;");
    flowLabel = new QLabel("Поток: -- л/мин");
    flowLabel->setStyleSheet("font-size: 16px; margin: 5px;");
    cLayout->addWidget(tempLabel);
    cLayout->addWidget(flowLabel);
    cLayout->addStretch();


    auto *sensorTab = new QWidget();
    auto *sLayout = new QVBoxLayout(sensorTab);
    
    auto *sHelpLayout = new QHBoxLayout();
    measSettingsBtn = new QPushButton("⚙");
    measSettingsBtn->setFixedSize(30, 30);
    measSettingsBtn->setStyleSheet(baseButtonStyle);
    connect(measSettingsBtn, &QPushButton::clicked, [this]() {
        SettingsDialog dlg("Измерения", this);
        dlg.exec();
    });
    faraday1Label = new QLabel("Цилиндр Фарадея 1: -- В");
    faraday1Label->setStyleSheet("font-size: 16px; margin: 5px;");
    sHelpLayout->addWidget(faraday1Label);
    sHelpLayout->addWidget(measSettingsBtn);

    faraday2Label = new QLabel("Цилиндр Фарадея 2: -- В");
    faraday2Label->setStyleSheet("font-size: 16px; margin: 5px;");

    hallLabel = new QLabel("Датчик Холла: -- мВ");
    hallLabel->setStyleSheet("font-size: 16px; margin: 5px;");
    
    vacuumVoltLabel = new QLabel("ВМБ-14 напряжение: -- В");
    vacuumVoltLabel->setStyleSheet("font-size: 16px; margin: 5px;");

    vacuumPressLabel = new QLabel("ВМБ-14 давление: -- Па");
    vacuumPressLabel->setStyleSheet("font-size: 16px; margin: 5px;");

    sLayout->addLayout(sHelpLayout);
    sLayout->addWidget(faraday2Label);
    sLayout->addWidget(hallLabel);
    sLayout->addWidget(vacuumVoltLabel);
    sLayout->addWidget(vacuumPressLabel);
    sLayout->addStretch();


    auto *historyTab = new QWidget();
    auto *historyMainLayout = new QVBoxLayout(historyTab);

    auto *topBarLayout = new QHBoxLayout();
    topBarLayout->addStretch();

    logSettingsBtn = new QPushButton("⚙");
    logSettingsBtn->setFixedSize(30, 30);
    logSettingsBtn->setStyleSheet(baseButtonStyle);
    connect(logSettingsBtn, &QPushButton::clicked, [this]() {
        SettingsDialog dlg("Логи", this); 
        dlg.exec();
    });
    
    topBarLayout->addWidget(logSettingsBtn);
    historyMainLayout->addLayout(topBarLayout);

    auto *hLayout = new QHBoxLayout();
    
    auto *textLogLayout = new QVBoxLayout();
    textLogLayout->addWidget(new QLabel("Текстовые логи (двойной клик для открытия):"));
    logFileList = new QListWidget();
    
    logFileList->setStyleSheet(
        "QListWidget { background-color: #1e1e1e; color: #dcdcdc; border: 1px solid #333; font-family: 'Consolas'; }"
        "QListWidget::item { padding: 5px; border-bottom: 1px solid #2a2a2a; }"
        "QListWidget::item:hover { background-color: #333; }"
    );

    connect(logFileList, &QListWidget::itemDoubleClicked, this, &MainWindow::onLogFileDoubleClicked);

    textLogLayout->addWidget(logFileList);

    auto *dataLogLayout = new QVBoxLayout();
    dataLogLayout->addWidget(new QLabel("Логи телеметрии (двойной клик - графики):"));
    dataFileList = new QListWidget();

    dataFileList->setStyleSheet(
        "QListWidget { background-color: #1e1e1e; color: #dcdcdc; border: 1px solid #333; font-family: 'Consolas'; }"
        "QListWidget::item { padding: 5px; border-bottom: 1px solid #2a2a2a; }"
        "QListWidget::item:hover { background-color: #333; }"
    );

    connect(dataFileList, &QListWidget::itemDoubleClicked, this, &MainWindow::onDataFileDoubleClicked);

    dataLogLayout->addWidget(dataFileList);
    
    hLayout->addLayout(textLogLayout);
    hLayout->addLayout(dataLogLayout);

    historyMainLayout->addLayout(hLayout);

    QPushButton *refreshBtn = new QPushButton("Обновить списки");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshLogList);
    
    historyMainLayout->addWidget(refreshBtn);


    tabs->addTab(powerTab, "Питание");
    tabs->addTab(coolingTab, "Охлаждение");
    tabs->addTab(sensorTab, "Измерения");
    tabs->addTab(historyTab, "Логи");


    mainLayout->addWidget(tabs);
    setCentralWidget(centralWidget);


    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFixedHeight(150);
    scrollArea->setStyleSheet("background-color: #1e1e1e; border: 1px solid #333;");

    logContainer = new QWidget();
    logLayout = new QVBoxLayout(logContainer);
    logLayout->setAlignment(Qt::AlignTop);
    logLayout->setContentsMargins(5, 5, 5, 5);
    logLayout->setSpacing(2);

    scrollArea->setWidget(logContainer);
    mainLayout->addWidget(scrollArea);

    setCentralWidget(centralWidget);

    refreshLogList();
}

void MainWindow::onLogMessage(const QString& msg) {
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString fullMsg = QString("[%1] %2").arg(timeStr, msg);

    QLabel *label = new QLabel(fullMsg);

    label->setWordWrap(true);
    label->setStyleSheet("padding: 3px; border-bottom: 1px solid #2a2a2a; color: #dcdcdc; font-family: 'Consolas', 'Monaco', monospace;");

    if (msg.contains("!")) {
        label->setStyleSheet(label->styleSheet() + "color: #ff6b6b; font-weight: bold;");
    }

    logLayout->insertWidget(0, label);

    QTimer::singleShot(10000, label, &QLabel::deleteLater);
}

void MainWindow::refreshLogList() {
    logFileList->clear();
    dataFileList->clear();
    
    QDir dir("Logs");
    if (!dir.exists()) return;

    dir.setFilter(QDir::Files | QDir::NoSymLinks);
    dir.setSorting(QDir::Time);
    
    QFileInfoList allFiles = dir.entryInfoList();

    for (const QFileInfo& fi : allFiles) {
        QString fileName = fi.fileName();
        
        if (fileName.startsWith("Data")) {
            dataFileList->addItem(fileName);
        } else {
            logFileList->addItem(fileName);
        }
    }
}

void MainWindow::onLogFileDoubleClicked(QListWidgetItem *item) {
    QString filePath = QDir::currentPath() + "/Logs/" + item->text();

    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void MainWindow::onDataFileDoubleClicked(QListWidgetItem *item) {
    QString filePath = QDir::currentPath() + "/Logs/" + item->text();

    auto* viewer = new DataViewerWindow(filePath, this);
    viewer->show();
}

void MainWindow::onTimerTick() {
    globalCurrent->setText(QString("Ток: %1 А").arg(systemManager->getCurrent(), 0, 'f', 2));
    globalTemp->setText(QString("Темп: %1 °C").arg(systemManager->getTemp(), 0, 'f', 1));

    uint8_t status = systemManager->getStatusFlags();
    bool isPowerOn = (status & 0x01);
    bool hasError = (status & 0x3E) != 0;

    QString ledStyle = "border-radius: 5px; min-width: 90px; min-height: 25px; font-weight: bold; font-size: 12px; color: white;";
    globalPowerLed->setStyleSheet(ledStyle + (isPowerOn ? "background-color: green;" : "background-color: gray;"));
    globalErrorLed->setStyleSheet(ledStyle + (hasError ? "background-color: red;" : "background-color: gray;"));

    auto setCol = [](QLabel* l, bool cond, const char* cOn, const char* cOff) {
        l->setStyleSheet(QString("border-radius:5px; min-width:90px; min-height:25px; font-weight: bold; font-size:10px; color:white; background-color: %1;").arg(cond ? cOn : cOff));
    };
    setCol(pumpLed, systemManager->getCoolState(), "lightgreen", "gray");
    setCol(radiatorLed, systemManager->getCoolState(), "lightgreen", "gray");
    tempLabel->setText(QString("Температура: %1 °C").arg(systemManager->getTemp()));
    flowLabel->setText(QString("Поток: %1 л/мин").arg(systemManager->getFlow()));

    if (systemManager->isPowerFresh(SettingsManager::instance().get("interface_freshness_limit").toInt())) {
        float cur = systemManager->getCurrent();
        float volt = systemManager->getAdcVoltage();

        currentValLabel->setText(QString("Текущий ток: %1 А").arg(cur, 0, 'f', 2));
        adcVoltLabel->setText(QString("Напряжение АЦП: %1 В").arg(volt, 0, 'f', 4));

        float real_time_axis = elapsedTimer.elapsed() / 1000.0f;
        
        ringBuffer.append(QPointF(real_time_axis, cur));
        if (ringBuffer.size() > 11 * updateFreq) ringBuffer.removeFirst(); 
        currentSeries->replace(ringBuffer);

        currentChart->axes(Qt::Horizontal).first()->setRange(real_time_axis - 10.0f, real_time_axis);
        time_axis += 1.0 / updateFreq;
    }
    updateLamps(systemManager->getStatusFlags());

    faraday1Label->setText(QString("Цилиндр Фарадея: \t %1 В").arg(systemManager->getFaraday1(), 0, 'f', 4));
    faraday2Label->setText(QString("Цилиндр Фарадея 2: \t %1 В").arg(systemManager->getFaraday2(), 0, 'f', 4));

    hallLabel->setText(QString("Датчик Холла: \t\t %1 мВ").arg(systemManager->getHall() * 1000.0f, 0, 'f', 2));
    
    float vacuum_v = systemManager->getVacuum();
    vacuumVoltLabel->setText(QString("Вакуум (Вольт): \t %1 В").arg(vacuum_v, 0, 'f', 4));

    vacuum_v = std::max(0.0f, std::min(10.0f, vacuum_v));
    auto optPressure = SensorController::getPressFromVolt(vacuum_v);
    if (optPressure.has_value()) {
        double pressure = optPressure.value();
        int exponent = std::floor(std::log10(pressure));
        double mantissa = pressure / std::pow(10.0, exponent);
        vacuumPressLabel->setText(QString("Вакуум (Давление): %1 * 10^%2 Па")
                            .arg(mantissa, 0, 'f', 2)
                            .arg(exponent));
    }
    else {
        vacuumPressLabel->setText(QString("Вакуум (Давление): \t Напр. вне рабочего диапазона"));
    }
}

void MainWindow::updateLamps(uint8_t status) {
    auto setCol = [](QLabel* l, bool cond, const char* cOn, const char* cOff) {
        l->setStyleSheet(QString("border-radius:5px; min-width:90px; min-height:25px; font-weight: bold; font-size:10px; color:white; background-color: %1;").arg(cond ? cOn : cOff));
    };
    
    setCol(powerLed,    (status & 0x01), "lightgreen", "gray"); 
    setCol(outProt1Led, (status & 0x02), "red", "gray");        
    setCol(outProt2Led, (status & 0x04), "red", "gray");        
    setCol(tempProtLed, (status & 0x08), "red", "gray");        
    setCol(invErrLed,   (status & 0x10), "red", "gray");        
    setCol(phaseErrLed, (status & 0x20), "red", "gray");
}

void MainWindow::onBusyStateChanged(bool isBusy) {
    startBtn->setEnabled(!systemManager->isOk() && !isBusy);
    stopBtn->setEnabled(systemManager->isOk() || isBusy); 
    setBtn->setEnabled(systemManager->isOk() && !isBusy);
    currentSpinBox->setEnabled(systemManager->isOk() && !isBusy);
}

void MainWindow::handleStart() { systemManager->startSystem(); }

void MainWindow::handleStop() { systemManager->stopSystem(); }

void MainWindow::handleSetCurrent() { systemManager->setCurrent(currentSpinBox->value()); }
