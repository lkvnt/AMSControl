#include <QHBoxLayout>
#include <QStatusBar>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>
#include <QGroupBox>
#include <QScrollArea>
#include "Manager.h"
#include "DataViewerWindow.h"
#include "QtUI.h"
#include "SettingsDialog.h"
#include "SettingsManager.h"
#include "Theme.h"

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
    this->setStyleSheet(Theme::getAppStylesheet());

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    auto *ctrlLayout = new QHBoxLayout();

    QString themeName = SettingsManager::instance().get("theme", "dark").toString();
    bool isDark = (themeName == "dark");

    QString baseButtonStyle = "";

    startBtn = new QPushButton("ЗАПУСК СИСТЕМЫ");
    startBtn->setStyleSheet(baseButtonStyle + 
        QString("QPushButton:enabled { background-color: %1; color: white; height: 35px; border: none; } "
                "QPushButton:hover:enabled { background-color: %2; }")
        .arg(isDark ? "#2E7D32" : "#388E3C")
        .arg(isDark ? "#4CAF50" : "#66BB6A"));
    
    stopBtn = new QPushButton("СТОП СИСТЕМЫ");
    stopBtn->setStyleSheet(baseButtonStyle + 
        QString("QPushButton:enabled { background-color: %1; color: white; height: 35px; border: none; } "
                "QPushButton:hover:enabled { background-color: %2; }")
        .arg(isDark ? "#C62828" : "#D32F2F")
        .arg(isDark ? "#F44336" : "#E57373"));

    mainSettingsBtn = new QPushButton("Настройки");
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


    QGroupBox *globalInfoBox = new QGroupBox();
    auto *gLayout = new QHBoxLayout(globalInfoBox);
    
    globalCurrent = new QLabel("Ток: -- А");
    globalCurrent->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(isDark ? "#4CAF50" : "#1E8449"));
    
    globalTemp = new QLabel("Темп: -- °C");
    globalTemp->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(isDark ? "#2196F3" : "#2980B9"));
    
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
    pLayout->setContentsMargins(15, 15, 15, 15);
    pLayout->setSpacing(12);
    
    auto *currLayout = new QHBoxLayout();
    currentSpinBox = new QDoubleSpinBox();
    currentSpinBox->setRange(0, 300);
    setBtn = new QPushButton("Установить ток");
    setBtn->setStyleSheet(baseButtonStyle);
    connect(setBtn, &QPushButton::clicked, this, &MainWindow::handleSetCurrent);
    powSettingsBtn = new QPushButton("Настройки");
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
    QString ledStyle = QString("border-radius: 12px; min-width: 120px; min-height: 25px; background-color: %1; color: %2; font-weight: bold; font-size: 11px; qproperty-alignment: 'AlignCenter';")
                           .arg(isDark ? "#444" : "#dcdcdc")
                           .arg(isDark ? "white" : "black");
    
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
    currentChart->setTheme(isDark ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    currentChart->setBackgroundVisible(false);
    QPen pen(isDark ? QColor("#4CAF50") : QColor("#1E8449"));
    pen.setWidth(2);
    currentSeries->setPen(pen);
    currentChart->legend()->hide();

    chartView = new QChartView(currentChart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setFrameShape(QFrame::NoFrame);
    chartView->setStyleSheet("background: transparent;");
    chartView->setMinimumHeight(250);
    pLayout->addWidget(chartView);

    auto *valLayout = new QHBoxLayout();
    currentValLabel = new QLabel("Текущий ток: -- А");
    currentValLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(isDark ? "#4CAF50" : "#1E8449"));
    adcVoltLabel = new QLabel("Напряжение АЦП: -- В");
    adcVoltLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(isDark ? "#2196F3" : "#2980B9"));
    valLayout->addWidget(currentValLabel);
    valLayout->addWidget(adcVoltLabel);
    pLayout->addLayout(valLayout);


    auto *coolingTab = new QWidget();
    auto *cLayout = new QVBoxLayout(coolingTab);
    cLayout->setContentsMargins(15, 15, 15, 15);
    cLayout->setSpacing(12);

    auto *cStatusLayout = new QHBoxLayout();
    pumpLed = new QLabel("НАСОС"); pumpLed->setStyleSheet(ledStyle);
    radiatorLed = new QLabel("РАДИАТОР"); radiatorLed->setStyleSheet(ledStyle);
    cStatusLayout->addWidget(pumpLed);
    cStatusLayout->addWidget(radiatorLed);

    coolSettingsBtn = new QPushButton("Настройки");
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
    sLayout->setContentsMargins(15, 15, 15, 15);
    sLayout->setSpacing(12);
    
    auto *sHelpLayout = new QHBoxLayout();
    measSettingsBtn = new QPushButton("Настройки");
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
    auto *historyTabLayout = new QVBoxLayout(historyTab);
    historyTabLayout->setContentsMargins(0, 0, 0, 0);

    historyStackedWidget = new QStackedWidget();
    historyTabLayout->addWidget(historyStackedWidget);

    historyListWidget = new QWidget();
    auto *historyMainLayout = new QVBoxLayout(historyListWidget);
    historyMainLayout->setContentsMargins(15, 15, 15, 15);
    historyMainLayout->setSpacing(12);

    auto *topBarLayout = new QHBoxLayout();
    topBarLayout->addStretch();

    logSettingsBtn = new QPushButton("Настройки");
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
    
    QString listStyle = isDark ?
                        "QListWidget { background-color: #1a1a1a; color: #a9b7c6; border: 1px solid #333; border-radius: 6px; font-family: 'Consolas', monospace; font-size: 13px; padding: 5px; } "
                        "QListWidget::item { padding: 6px; border-bottom: 1px solid #2a2a2a; border-radius: 4px; } "
                        "QListWidget::item:hover { background-color: #2a2a2a; } "
                        "QListWidget::item:selected { background-color: #2196F3; color: white; }"
                        :
                        "QListWidget { background-color: #ffffff; color: #333; border: 1px solid #c5c5c5; border-radius: 6px; font-family: 'Consolas', monospace; font-size: 13px; padding: 5px; } "
                        "QListWidget::item { padding: 6px; border-bottom: 1px solid #e1e1e1; border-radius: 4px; } "
                        "QListWidget::item:hover { background-color: #f0f0f0; } "
                        "QListWidget::item:selected { background-color: #007bff; color: white; }";

    logFileList->setStyleSheet(listStyle);

    connect(logFileList, &QListWidget::itemDoubleClicked, this, &MainWindow::onLogFileDoubleClicked);

    textLogLayout->addWidget(logFileList);

    auto *dataLogLayout = new QVBoxLayout();
    dataLogLayout->addWidget(new QLabel("Логи телеметрии (двойной клик - графики):"));
    dataFileList = new QListWidget();

    dataFileList->setStyleSheet(listStyle);

    connect(dataFileList, &QListWidget::itemDoubleClicked, this, &MainWindow::onDataFileDoubleClicked);

    dataLogLayout->addWidget(dataFileList);
    
    hLayout->addLayout(textLogLayout);
    hLayout->addLayout(dataLogLayout);

    historyMainLayout->addLayout(hLayout);

    QPushButton *refreshBtn = new QPushButton("Обновить списки");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshLogList);
    
    historyMainLayout->addWidget(refreshBtn);

    historyViewWidget = new QWidget();
    auto *viewLayout = new QVBoxLayout(historyViewWidget);
    viewLayout->setContentsMargins(15, 15, 15, 15);
    viewLayout->setSpacing(12);

    auto *closeLogBtn = new QPushButton("Закрыть лог и вернуться");
    closeLogBtn->setStyleSheet(baseButtonStyle);
    connect(closeLogBtn, &QPushButton::clicked, [this]() {
        historyStackedWidget->setCurrentWidget(historyListWidget);
        logFileTextEdit->clear();
    });

    logFileTextEdit = new QTextEdit();
    logFileTextEdit->setReadOnly(true);
    logFileTextEdit->setStyleSheet(QString("QTextEdit { background-color: %1; color: %2; border: 1px solid %3; border-radius: 6px; font-family: 'Consolas', monospace; font-size: 13px; padding: 5px; }")
                                   .arg(isDark ? "#1a1a1a" : "#ffffff")
                                   .arg(isDark ? "#a9b7c6" : "#333333")
                                   .arg(isDark ? "#333" : "#c5c5c5"));

    viewLayout->addWidget(closeLogBtn);
    viewLayout->addWidget(logFileTextEdit);

    historyStackedWidget->addWidget(historyListWidget);
    historyStackedWidget->addWidget(historyViewWidget);
    historyStackedWidget->setCurrentWidget(historyListWidget);

    tabs->addTab(powerTab, "Питание");
    tabs->addTab(coolingTab, "Охлаждение");
    tabs->addTab(sensorTab, "Измерения");
    tabs->addTab(historyTab, "Логи");


    mainLayout->addWidget(tabs);
    setCentralWidget(centralWidget);


    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setObjectName("logScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFixedHeight(150);
    scrollArea->viewport()->setStyleSheet("background: transparent;");
    scrollArea->setStyleSheet(QString("QScrollArea { background-color: %1; border: 1px solid %2; border-radius: 6px; margin-top: 5px; }")
                              .arg(isDark ? "#1a1a1a" : "#ffffff")
                              .arg(isDark ? "#333" : "#c5c5c5"));

    logContainer = new QWidget();
    logContainer->setObjectName("logContainer");
    logContainer->setStyleSheet(QString("#logContainer { background-color: %1; }").arg(isDark ? "#1a1a1a" : "#ffffff"));
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
    QString themeName = SettingsManager::instance().get("theme", "dark").toString();
    bool isDark = (themeName == "dark");
    label->setStyleSheet(QString("padding: 4px; border-bottom: 1px solid %1; color: %2; font-family: 'Consolas', 'Monaco', monospace; font-size: 12px; background: transparent;")
                         .arg(isDark ? "#2a2a2a" : "#e1e1e1")
                         .arg(isDark ? "#a9b7c6" : "#333333"));

    if (msg.contains("!")) {
        QString errorColor = isDark ? "#F44336" : "#D32F2F";
        label->setStyleSheet(label->styleSheet() + QString("color: %1; font-weight: bold;").arg(errorColor));
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

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QString themeName = SettingsManager::instance().get("theme", "dark").toString();
    bool isDark = (themeName == "dark");
    QString errorColor = isDark ? "#F44336" : "#D32F2F";

    QTextStream in(&file);
    QStringList htmlLines;
    htmlLines.append("<div style='white-space: pre-wrap;'>");
    while (!in.atEnd()) {
        QString line = in.readLine().toHtmlEscaped();
        if (line.contains("!")) {
            htmlLines.append(QString("<span style='color: %1; font-weight: bold;'>").arg(errorColor) + line + "</span><br>");
        } else {
            htmlLines.append(line + "<br>");
        }
    }
    htmlLines.append("</div>");
    file.close();

    logFileTextEdit->setHtml(htmlLines.join(""));
    historyStackedWidget->setCurrentWidget(historyViewWidget);
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

    QString themeName = SettingsManager::instance().get("theme", "dark").toString();
    bool isDark = (themeName == "dark");
    const char* colorOn = isDark ? "#4CAF50" : "#2E7D32";
    const char* colorOff = isDark ? "#444" : "#dcdcdc";
    const char* colorErr = isDark ? "#F44336" : "#C62828";
    const char* textColor = isDark ? "white" : "black";

    QString globalLedStyle = QString("border-radius: 12px; min-width: 90px; min-height: 25px; font-weight: bold; font-size: 11px; color: %1; qproperty-alignment: 'AlignCenter';").arg(textColor);
    globalPowerLed->setStyleSheet(globalLedStyle + QString("background-color: %1;").arg(isPowerOn ? colorOn : colorOff));
    globalErrorLed->setStyleSheet(globalLedStyle + QString("background-color: %1;").arg(hasError ? colorErr : colorOff));

    auto setCol = [textColor](QLabel* l, bool cond, const char* cOn, const char* cOff) {
        l->setStyleSheet(QString("border-radius: 12px; min-width: 90px; min-height: 25px; font-weight: bold; font-size: 11px; color: %1; qproperty-alignment: 'AlignCenter'; background-color: %2;").arg(textColor).arg(cond ? cOn : cOff));
    };
    setCol(pumpLed, systemManager->getCoolState(), colorOn, colorOff);
    setCol(radiatorLed, systemManager->getCoolState(), colorOn, colorOff);
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
    
    float vacuum_v = systemManager->getVacuumVoltage();
    auto vacuum_p = systemManager->getVacuumPressure();
    vacuumVoltLabel->setText(QString("Вакуум (Вольт): \t %1 В").arg(vacuum_v, 0, 'f', 4));

    vacuumPressLabel->setText(QString("Вакуум (Давление): ") + systemManager->formateVacuumValue(vacuum_p));
}

void MainWindow::updateLamps(uint8_t status) {
    QString themeName = SettingsManager::instance().get("theme", "dark").toString();
    bool isDark = (themeName == "dark");
    const char* colorOn = isDark ? "#4CAF50" : "#2E7D32";
    const char* colorErr = isDark ? "#F44336" : "#C62828";
    const char* colorOff = isDark ? "#444" : "#dcdcdc";
    const char* textColor = isDark ? "white" : "black";

    auto setCol = [textColor](QLabel* l, bool cond, const char* cOn, const char* cOff) {
        l->setStyleSheet(QString("border-radius: 12px; min-width: 90px; min-height: 25px; font-weight: bold; font-size: 11px; color: %1; qproperty-alignment: 'AlignCenter'; background-color: %2;").arg(textColor).arg(cond ? cOn : cOff));
    };
    
    setCol(powerLed,    (status & 0x01), colorOn, colorOff); 
    setCol(outProt1Led, (status & 0x02), colorErr, colorOff);        
    setCol(outProt2Led, (status & 0x04), colorErr, colorOff);        
    setCol(tempProtLed, (status & 0x08), colorErr, colorOff);        
    setCol(invErrLed,   (status & 0x10), colorErr, colorOff);        
    setCol(phaseErrLed, (status & 0x20), colorErr, colorOff);
}

void MainWindow::applyTheme() {
    this->setStyleSheet(Theme::getAppStylesheet());
    
    QString themeName = SettingsManager::instance().get("theme", "dark").toString();
    bool isDark = (themeName == "dark");

    startBtn->setStyleSheet(
        QString("QPushButton:enabled { background-color: %1; color: white; height: 35px; border: none; } "
                "QPushButton:hover:enabled { background-color: %2; }")
        .arg(isDark ? "#2E7D32" : "#388E3C")
        .arg(isDark ? "#4CAF50" : "#66BB6A"));
    
    stopBtn->setStyleSheet(
        QString("QPushButton:enabled { background-color: %1; color: white; height: 35px; border: none; } "
                "QPushButton:hover:enabled { background-color: %2; }")
        .arg(isDark ? "#C62828" : "#D32F2F")
        .arg(isDark ? "#F44336" : "#E57373"));

    globalCurrent->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(isDark ? "#4CAF50" : "#1E8449"));
    globalTemp->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(isDark ? "#2196F3" : "#2980B9"));
    
    // --- Радикальное пересоздание графика для исправления "призрачной" линии в OpenGL ---

    // 1. Создаем новый график и настраиваем его
    auto* newChart = new QChart();
    newChart->setTheme(isDark ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    newChart->setBackgroundVisible(false);
    newChart->legend()->hide();
    newChart->setTitle("Мониторинг тока (А)");

    // 2. Создаем новую линию, стилизуем и загружаем в нее старые данные
    auto* newSeries = new QLineSeries();
    newSeries->setUseOpenGL(true);
    QPen pen(isDark ? QColor("#4CAF50") : QColor("#1E8449"));
    pen.setWidth(2);
    newSeries->setPen(pen);
    newSeries->replace(ringBuffer);

    // 3. Добавляем линию на новый график и настраиваем оси
    newChart->addSeries(newSeries);
    newChart->createDefaultAxes();
    newChart->axes(Qt::Vertical).first()->setRange(0, 350);
    if (!ringBuffer.isEmpty()) {
        newChart->axes(Qt::Horizontal).first()->setRange(ringBuffer.last().x() - 10.0f, ringBuffer.last().x());
    } else {
        newChart->axes(Qt::Horizontal).first()->setRange(time_axis - 10.0f, time_axis);
    }
    
    // 4. Подменяем график во вьюпорте и удаляем старые объекты
    chartView->setChart(newChart);
    delete currentChart; // Старый график удаляется, унося с собой старую линию
    currentChart = newChart;
    currentSeries = newSeries;

    currentValLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(isDark ? "#4CAF50" : "#1E8449"));
    adcVoltLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(isDark ? "#2196F3" : "#2980B9"));

    QString listStyle = isDark ?
                        "QListWidget { background-color: #1a1a1a; color: #a9b7c6; border: 1px solid #333; border-radius: 6px; font-family: 'Consolas', monospace; font-size: 13px; padding: 5px; } "
                        "QListWidget::item { padding: 6px; border-bottom: 1px solid #2a2a2a; border-radius: 4px; } "
                        "QListWidget::item:hover { background-color: #2a2a2a; } "
                        "QListWidget::item:selected { background-color: #2196F3; color: white; }"
                        :
                        "QListWidget { background-color: #ffffff; color: #333; border: 1px solid #c5c5c5; border-radius: 6px; font-family: 'Consolas', monospace; font-size: 13px; padding: 5px; } "
                        "QListWidget::item { padding: 6px; border-bottom: 1px solid #e1e1e1; border-radius: 4px; } "
                        "QListWidget::item:hover { background-color: #f0f0f0; } "
                        "QListWidget::item:selected { background-color: #007bff; color: white; }";

    logFileList->setStyleSheet(listStyle);
    dataFileList->setStyleSheet(listStyle);

    logFileTextEdit->setStyleSheet(QString("QTextEdit { background-color: %1; color: %2; border: 1px solid %3; border-radius: 6px; font-family: 'Consolas', monospace; font-size: 13px; padding: 5px; }")
                                   .arg(isDark ? "#1a1a1a" : "#ffffff")
                                   .arg(isDark ? "#a9b7c6" : "#333333")
                                   .arg(isDark ? "#333" : "#c5c5c5"));

    if (auto scrollArea = this->findChild<QScrollArea*>("logScrollArea")) {
        scrollArea->setStyleSheet(QString("QScrollArea { background-color: %1; border: 1px solid %2; border-radius: 6px; margin-top: 5px; }")
                                  .arg(isDark ? "#1a1a1a" : "#ffffff")
                                  .arg(isDark ? "#333" : "#c5c5c5"));
    }
    
    logContainer->setStyleSheet(QString("#logContainer { background-color: %1; }").arg(isDark ? "#1a1a1a" : "#ffffff"));
    
    for (int i = 0; i < logLayout->count(); ++i) {
        if (auto label = qobject_cast<QLabel*>(logLayout->itemAt(i)->widget())) {
            label->setStyleSheet(QString("padding: 4px; border-bottom: 1px solid %1; color: %2; font-family: 'Consolas', 'Monaco', monospace; font-size: 12px; background: transparent;")
                                 .arg(isDark ? "#2a2a2a" : "#e1e1e1")
                                 .arg(isDark ? "#a9b7c6" : "#333333"));
            if (label->text().contains("!")) {
                QString errorColor = isDark ? "#F44336" : "#D32F2F";
                label->setStyleSheet(label->styleSheet() + QString("color: %1; font-weight: bold;").arg(errorColor));
            }
        }
    }
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
