#include "QtUI.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), time_axis(0.0f) {
    setupUI();
    
    resize(800, 600);

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::onTimerTick);
    updateTimer->start(100); // Опрос каждые 100мс для плавной линии

    manager.startSystem(); 
}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    auto *tabs = new QTabWidget();

    // --- Вкладка Питания ---
    auto *powerTab = new QWidget();
    auto *pLayout = new QVBoxLayout(powerTab);
    
    // Блок установки тока
    auto *ctrlLayout = new QHBoxLayout();
    currentSpinBox = new QDoubleSpinBox();
    currentSpinBox->setRange(-300, 300);
    auto *setBtn = new QPushButton("Установить ток");
    connect(setBtn, &QPushButton::clicked, this, &MainWindow::handleSetCurrent);
    ctrlLayout->addWidget(new QLabel("Целевой ток (А):"));
    ctrlLayout->addWidget(currentSpinBox);
    ctrlLayout->addWidget(setBtn);
    pLayout->addLayout(ctrlLayout);

    // Блок лампочек
    auto *statusLayout = new QHBoxLayout();
    QString ledStyle = "border-radius: 10px; min-width: 20px; min-height: 20px; background-color: gray;";
    powerLed = new QLabel("ПИТАНИЕ"); powerLed->setStyleSheet(ledStyle); powerLed->setAlignment(Qt::AlignCenter);
    phaseErrLed = new QLabel("ОШИБКА ФАЗ"); phaseErrLed->setStyleSheet(ledStyle); phaseErrLed->setAlignment(Qt::AlignCenter);
    invErrLed = new QLabel("ОШИБКА ИНВ"); invErrLed->setStyleSheet(ledStyle); invErrLed->setAlignment(Qt::AlignCenter);
    
    statusLayout->addWidget(powerLed);
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
    chartView->setMinimumHeight(300);
    pLayout->addWidget(chartView);

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

    auto *stopBtn = new QPushButton("ОБЩИЙ АВАРИЙНЫЙ СТОП");
    stopBtn->setStyleSheet("background-color: darkred; color: white; height: 50px; font-weight: bold;");
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::handleEmergency);

    mainLayout->addWidget(tabs);
    mainLayout->addWidget(stopBtn);
    setCentralWidget(centralWidget);
}

void MainWindow::onTimerTick() {
    manager.update(); 
    
    tempLabel->setText(QString("Температура: %1 °C").arg(manager.getTemp()));
    flowLabel->setText(QString("Поток: %1 л/мин").arg(manager.getFlow()));

    // Обновление графика
    float cur = manager.getCurrent();
    currentSeries->append(time_axis, cur);
    if (currentSeries->count() > 100) currentSeries->remove(0); 
    currentChart->axes(Qt::Horizontal).first()->setRange(time_axis - 10, time_axis);
    time_axis += 0.1f;

    updateLamps(manager.getStatusFlags());
}

void MainWindow::updateLamps(uint8_t status) {
    auto setCol = [](QLabel* l, bool cond, const char* cOn, const char* cOff) {
        l->setStyleSheet(QString("border-radius:10px; min-width:20px; min-height:20px; background-color: %1;").arg(cond ? cOn : cOff));
    };
    
    setCol(powerLed, (status & 0x01), "lightgreen", "gray");   // 0 бит - статус включения
    setCol(invErrLed, (status & 0x08), "red", "gray");         // 3 бит - защита инвертора
    setCol(phaseErrLed, (status & 0x10), "red", "gray");       // 4 бит - защита фаз
}

void MainWindow::handleSetCurrent() {
    manager.setCurrent(currentSpinBox->value());
}

void MainWindow::handleEmergency() {
    manager.emergencyAllStop();
}