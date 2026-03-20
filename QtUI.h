#ifndef QTUI_H
#define QTUI_H

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QTimer>
#include "Manager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

private slots:
    void onTimerTick();          // Обновление данных на экране
    void handleSetCurrent();     // Кнопка установки тока
    void handleEmergency();      // Кнопка общего стопа

private:
    void setupUI();
    
    SystemManager manager;
    QTimer *updateTimer;

    // Виджеты для отображения данных
    QLabel *tempLabel;
    QLabel *flowLabel;
    QDoubleSpinBox *currentSpinBox;
};

#endif