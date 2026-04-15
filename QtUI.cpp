#include "QtUI.h"
#include <QHBoxLayout>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), time_axis(0.0f) {
    setupUI();
    resize(800, 600);

    // Подписываемся на логи Менеджера и выводим в StatusBar
    connect(&manager, &SystemManager::logMessage, this, &MainWindow::onLogMessage);

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::onTimerTick);
    updateTimer->start(100); // Опрос каждые 100мс для плавной линии
}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    // --- Панель управления запуском ---
    auto *ctrlLayout = new QHBoxLayout();
    startBtn = new QPushButton("ЗАПУСК СИСТЕМЫ");
    startBtn->setStyleSheet("background-color: #2e8b57; color: white; height: 35px; font-weight: bold; border-radius: 5px;");
    
    stopBtn = new QPushButton("ПЛАНОВЫЙ СТОП");
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
    auto *setBtn = new QPushButton("Установить ток");
    connect(setBtn, &QPushButton::clicked, this, &MainWindow::handleSetCurrent);
    currLayout->addWidget(new QLabel("Целевой ток (А):"));
    currLayout->addWidget(currentSpinBox);
    currLayout->addWidget(setBtn);
    pLayout->addLayout(currLayout);

    // Блок лампочек
    auto *statusLayout = new QHBoxLayout();
    QString ledStyle = "border-radius: 5px; min-width: 120px; min-height: 25px; background-color: gray; color: white; font-weight: bold; qproperty-alignment: 'AlignCenter';";
    powerLed = new QLabel("ПИТАНИЕ"); powerLed->setStyleSheet(ledStyle);
    phaseErrLed = new QLabel("ОШИБКА ФАЗ"); phaseErrLed->setStyleSheet(ledStyle);
    invErrLed = new QLabel("ОШИБКА ИНВ"); invErrLed->setStyleSheet(ledStyle);
    
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

    auto *emerStopBtn = new QPushButton("ОБЩИЙ АВАРИЙНЫЙ СТОП");
    emerStopBtn->setStyleSheet("background-color: darkred; color: white; height: 50px; font-weight: bold; border-radius: 5px;");
    connect(emerStopBtn, &QPushButton::clicked, this, &MainWindow::handleEmergency);

    mainLayout->addWidget(tabs);
    mainLayout->addWidget(emerStopBtn);
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
    // Создаем виджет для строки лога
    // QLabel *label = new QLabel(QString("[%1] %2")
    //                             .arg(QTime::currentTime().toString("hh:mm:ss"))
    //                             .arg(msg));
    
    QLabel *label = new QLabel(QString("%1").arg(msg));

    label->setWordWrap(true);
    label->setStyleSheet("padding: 3px; border-bottom: 1px solid #2a2a2a; color: #dcdcdc; font-family: 'Consolas', 'Monaco', monospace;");

    // Выделяем ошибки цветом
    if (msg.contains("ОШИБКА") || msg.contains("АВАРИЯ") || msg.contains("Ошибка")) {
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
    currentSeries->append(time_axis, cur);
    if (currentSeries->count() > 100) currentSeries->remove(0); 
    currentChart->axes(Qt::Horizontal).first()->setRange(time_axis - 10, time_axis);
    time_axis += 0.1f;

    updateLamps(manager.getStatusFlags());

    // Визуальная подсветка кнопок
    startBtn->setEnabled(!manager.isOk());
    stopBtn->setEnabled(manager.isOk());
}

void MainWindow::updateLamps(uint8_t status) {
    auto setCol = [](QLabel* l, bool cond, const char* cOn, const char* cOff) {
        l->setStyleSheet(QString("border-radius:10px; min-width:20px; min-height:20px; background-color: %1;").arg(cond ? cOn : cOff));
    };
    
    setCol(powerLed, (status & 0x01), "lightgreen", "gray");   // 0 бит - статус включения
    setCol(invErrLed, (status & 0x10), "red", "gray");         // 4 бит - защита инвертора
    setCol(phaseErrLed, (status & 0x20), "red", "gray");       // 5 бит - защита фаз
}

void MainWindow::handleStart() {
    manager.startSystem();
}

void MainWindow::handleStop() {
    manager.stopSystem();
}

void MainWindow::handleSetCurrent() {
    manager.setCurrent(currentSpinBox->value());
}

void MainWindow::handleEmergency() {
    manager.emergencyAllStop();
}