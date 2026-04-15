#include "QtUI.h"
#include <QHBoxLayout>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), time_axis(0.0f), updateFreq(10.0f) {
    setupUI();
    resize(900, 700);

    // Подписываемся на логи Менеджера и выводим в StatusBar
    connect(&manager, &SystemManager::logMessage, this, &MainWindow::onLogMessage);

    // Подписываемся на сигнал занятости
    connect(&manager, &SystemManager::busyStateChanged, this, &MainWindow::onBusyStateChanged);

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::onTimerTick);
    updateTimer->start(1000.0 / updateFreq);
}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    // --- Панель управления запуском ---
    auto *ctrlLayout = new QHBoxLayout();
    startBtn = new QPushButton("ЗАПУСК СИСТЕМЫ");
    startBtn->setStyleSheet("background-color: #2e8b57; color: white; height: 35px; font-weight: bold; border-radius: 5px;");
    
    stopBtn = new QPushButton("СТОП СИСТЕМЫ");
    stopBtn->setStyleSheet("background-color: #d2691e; color: white; height: 35px; font-weight: bold; border-radius: 5px;");
    
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::handleStart);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::handleStop);
    
    ctrlLayout->addWidget(startBtn);
    ctrlLayout->addWidget(stopBtn);
    mainLayout->addLayout(ctrlLayout);

    auto *tabs = new QTabWidget();

    // --- Вкладка Питания ---
    auto *powerTab = new QWidget();
    auto *pLayout = new QVBoxLayout(powerTab);
    
    // Блок установки тока
    auto *currLayout = new QHBoxLayout();
    currentSpinBox = new QDoubleSpinBox();
    currentSpinBox->setRange(0, 300);
    setBtn = new QPushButton("Установить ток");
    connect(setBtn, &QPushButton::clicked, this, &MainWindow::handleSetCurrent);
    currLayout->addWidget(new QLabel("Целевой ток (А):"));
    currLayout->addWidget(currentSpinBox);
    currLayout->addWidget(setBtn);
    pLayout->addLayout(currLayout);

    // Блок лампочек
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

    // Блок графика
    currentSeries = new QLineSeries();
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

    // Текущие значения (под графиком)
    auto *valLayout = new QHBoxLayout();
    currentValLabel = new QLabel("Текущий ток: -- А");
    currentValLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #4CAF50;");
    adcVoltLabel = new QLabel("Напряжение АЦП: -- В");
    adcVoltLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #2196F3;");
    valLayout->addWidget(currentValLabel);
    valLayout->addWidget(adcVoltLabel);
    pLayout->addLayout(valLayout);

    // --- Вкладка Охлаждения ---
    auto *coolingTab = new QWidget();
    auto *cLayout = new QVBoxLayout(coolingTab);
    tempLabel = new QLabel("Температура: -- °C");
    flowLabel = new QLabel("Поток: -- л/мин");
    cLayout->addWidget(tempLabel);
    cLayout->addWidget(flowLabel);
    cLayout->addStretch();

    tabs->addTab(powerTab, "Питание");
    tabs->addTab(coolingTab, "Охлаждение");


    mainLayout->addWidget(tabs);
    setCentralWidget(centralWidget);

    // --- Уведомления ---
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFixedHeight(150); // Фиксированная высота "окна" логов
    scrollArea->setStyleSheet("background-color: #1e1e1e; border: 1px solid #333;");

    logContainer = new QWidget();
    logLayout = new QVBoxLayout(logContainer);
    logLayout->setAlignment(Qt::AlignTop); // Новые логи будут прижиматься к верху
    logLayout->setContentsMargins(5, 5, 5, 5);
    logLayout->setSpacing(2);

    scrollArea->setWidget(logContainer);
    mainLayout->addWidget(scrollArea); // Добавляем в самый низ главного компоновщика

    setCentralWidget(centralWidget);
}

void MainWindow::onLogMessage(const QString& msg) {
    QLabel *label = new QLabel(QString("%1").arg(msg));

    label->setWordWrap(true);
    label->setStyleSheet("padding: 3px; border-bottom: 1px solid #2a2a2a; color: #dcdcdc; font-family: 'Consolas', 'Monaco', monospace;");

    // Выделяем ошибки цветом
    if (msg.contains("!")) {
        label->setStyleSheet(label->styleSheet() + "color: #ff6b6b; font-weight: bold;");
    }

    // Добавляем в начало списка (новое сверху)
    logLayout->insertWidget(0, label);

    // Таймер самоуничтожения через 5 секунд
    // После удаления виджета Layout автоматически "подтянет" остальные элементы вверх
    QTimer::singleShot(10000, label, &QLabel::deleteLater);
    
    // Также если надо дублируем в консоль для отладки (без буферизации)
    // std::cout << label->text().toLocal8Bit().constData() << std::endl;
}

void MainWindow::onTimerTick() {
    manager.update(); 
    
    tempLabel->setText(QString("Температура: %1 °C").arg(manager.getTemp()));
    flowLabel->setText(QString("Поток: %1 л/мин").arg(manager.getFlow()));

    // Обновление графика
    float cur = manager.getCurrent();
    float volt = manager.getAdcVoltage();

    currentValLabel->setText(QString("Текущий ток: %1 А").arg(cur, 0, 'f', 2));
    adcVoltLabel->setText(QString("Напряжение АЦП: %1 В").arg(volt, 0, 'f', 4));

    currentSeries->append(time_axis, cur);
    if (currentSeries->count() > 100) currentSeries->remove(0); 
    currentChart->axes(Qt::Horizontal).first()->setRange(time_axis - 10, time_axis);
    time_axis += 1.0 / updateFreq;

    updateLamps(manager.getStatusFlags());
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
    startBtn->setEnabled(!manager.isOk() && !isBusy);
    stopBtn->setEnabled(manager.isOk() || isBusy); 
    setBtn->setEnabled(manager.isOk() && !isBusy);
    currentSpinBox->setEnabled(manager.isOk() && !isBusy);
}

void MainWindow::handleStart() { manager.startSystem(); }

void MainWindow::handleStop() { manager.stopSystem(); }

void MainWindow::handleSetCurrent() { manager.setCurrent(currentSpinBox->value()); }
