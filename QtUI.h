#ifndef QTUI_H
#define QTUI_H

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QVBoxLayout>
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
    void handleStart();
    void handleStop();
    void onLogMessage(const QString& msg);

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

    QPushButton *startBtn;
    QPushButton *stopBtn;

    QLineSeries *currentSeries;
    QChart *currentChart;
    float time_axis;

    QVBoxLayout *logLayout;
    QWidget *logContainer;
};

#endif // QTUI_H