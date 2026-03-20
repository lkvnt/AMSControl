#include "QtUI.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::onTimerTick);
    updateTimer->start(500); // Опрос раз в полсекунды

    manager.startSystem(); // Автозапуск охлаждения при старте ПО
}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    auto *tabs = new QTabWidget();

    // --- Вкладка Питания ---
    auto *powerTab = new QWidget();
    auto *pLayout = new QVBoxLayout(powerTab);
    currentSpinBox = new QDoubleSpinBox();
    auto *setBtn = new QPushButton("Установить ток");
    connect(setBtn, &QPushButton::clicked, this, &MainWindow::handleSetCurrent);
    
    pLayout->addWidget(new QLabel("Целевой ток (А):"));
    pLayout->addWidget(currentSpinBox);
    pLayout->addWidget(setBtn);
    pLayout->addStretch();

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

    // Глобальная кнопка стопа
    auto *stopBtn = new QPushButton("ОБЩИЙ АВАРИЙНЫЙ СТОП");
    stopBtn->setStyleSheet("background-color: darkred; color: white; height: 50px; font-weight: bold;");
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::handleEmergency);

    mainLayout->addWidget(tabs);
    mainLayout->addWidget(stopBtn);
    setCentralWidget(centralWidget);
}

void MainWindow::onTimerTick() {
    manager.update(); // Проверка безопасности
    
    // Обновление UI данными из менеджера
    tempLabel->setText(QString("Температура: %1 °C").arg(manager.getTemp()));
    flowLabel->setText(QString("Поток: %1 л/мин").arg(manager.getFlow()));
}

void MainWindow::handleSetCurrent() {
    manager.setCurrent(currentSpinBox->value());
}

void MainWindow::handleEmergency() {
    manager.emergencyAllStop();
}