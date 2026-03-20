#ifndef QTUI_H
#define QTUI_H

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include "Manager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void onTimerTick();          
    void handleSetCurrent();     
    void handleEmergency();      

private:
    void setupUI();
    void updateLamps(uint8_t status);
    
    SystemManager manager;
    QTimer *updateTimer;

    QLabel *tempLabel;
    QLabel *flowLabel;
    QDoubleSpinBox *currentSpinBox;
    
    // Элементы статуса и графиков
    QLabel *powerLed;
    QLabel *phaseErrLed;
    QLabel *invErrLed;
    QLineSeries *currentSeries;
    QChart *currentChart;
    float time_axis;
};

#endif // QTUI_H