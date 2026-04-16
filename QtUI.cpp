#include <QHBoxLayout>
#include <QStatusBar>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>
#include "QtUI.h"

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

    onBusyStateChanged(false);
    QTimer::singleShot(200, this, [this]() {
        manager.initHardware();
    });
}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    // --- Панель управления запуском ---
    auto *ctrlLayout = new QHBoxLayout();

    QString baseButtonStyle = 
        "QPushButton {"
        "  border: 1px solid #555;"
        "  border-radius: 5px;"
        "  padding: 5px;"
        "  background-color: #444;" // Базовый цвет для обычных кнопок
        "  color: white;"
        "}"
        "QPushButton:hover:enabled {"
        "  background-color: #5a5a5a;" // Подсветка при наведении для обычных кнопок
        "  border: 1px solid #888;"    // Светлая рамка при наведении
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
    
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::handleStart);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::handleStop);
    
    // stopBtn->setEnabled(false);

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
    setBtn->setStyleSheet(baseButtonStyle);
    connect(setBtn, &QPushButton::clicked, this, &MainWindow::handleSetCurrent);
    // setBtn->setEnabled(false);
    // currentSpinBox->setEnabled(false);
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

    // --- Вкладка Логов ---
    auto *historyTab = new QWidget();
    auto *hLayout = new QVBoxLayout(historyTab);
    
    QLabel *hLabel = new QLabel("История логов (двойной клик для открытия):");
    logFileList = new QListWidget();
    
    // Стилизуем список под темную тему
    logFileList->setStyleSheet(
        "QListWidget { background-color: #1e1e1e; color: #dcdcdc; border: 1px solid #333; font-family: 'Consolas'; }"
        "QListWidget::item { padding: 5px; border-bottom: 1px solid #2a2a2a; }"
        "QListWidget::item:hover { background-color: #333; }"
    );

    connect(logFileList, &QListWidget::itemDoubleClicked, this, &MainWindow::onLogFileDoubleClicked);

    hLayout->addWidget(hLabel);
    hLayout->addWidget(logFileList);
    
    // Добавляем кнопку обновления списка
    QPushButton *refreshBtn = new QPushButton("Обновить список");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshLogList);
    hLayout->addWidget(refreshBtn);

    tabs->addTab(powerTab, "Питание");
    tabs->addTab(coolingTab, "Охлаждение");
    tabs->addTab(historyTab, "Логи");


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

    refreshLogList();
}

void MainWindow::onLogMessage(const QString& msg) {
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString fullMsg = QString("[%1] %2").arg(timeStr, msg);

    QLabel *label = new QLabel(fullMsg);

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

    // Сохранение в файл
    saveLogToFile(fullMsg);
}

void MainWindow::saveLogToFile(const QString& formattedMsg) {
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

void MainWindow::refreshLogList() {
    logFileList->clear();
    
    QDir dir("Logs");
    if (!dir.exists()) return;

    // Получаем список .txt файлов, сортируем по дате (новые сверху)
    dir.setNameFilters(QStringList() << "*.txt");
    dir.setFilter(QDir::Files);
    dir.setSorting(QDir::Time);

    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); ++i) {
        logFileList->addItem(list.at(i).fileName());
    }
}

void MainWindow::onLogFileDoubleClicked(QListWidgetItem *item) {
    QString filePath = QDir::currentPath() + "/Logs/" + item->text();
    
    // Открываем файл встроенными средствами ОС (Блокнот, TextEdit и т.д.)
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
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
