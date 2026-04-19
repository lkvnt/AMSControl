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
#include <QListWidget>
#include "Manager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void onTimerTick();          
    void handleSetCurrent();     
    void handleStart();
    void handleStop();
    void onLogMessage(const QString& msg);
    void onLogFileDoubleClicked(QListWidgetItem *item);
    void refreshLogList();
    void onBusyStateChanged(bool isBusy);

private:
    void setupUI();
    void updateLamps(uint8_t status);
    void saveLogToFile(const QString& formattedMsg);
    
    SystemManager manager;
    QTimer *updateTimer;

    QLabel *tempLabel;
    QLabel *flowLabel;
    QDoubleSpinBox *currentSpinBox;
    QPushButton *setBtn;
    QLabel *faradayLabel;
    QLabel *hallLabel;
    QLabel *vacuumLabel;

    QLabel *currentValLabel;
    QLabel *adcVoltLabel;
    
    // Элементы статуса и графиков
    QLabel *globalCurrent;
    QLabel *globalTemp;
    QLabel *globalPowerLed;
    QLabel *globalErrorLed;
    QLabel *powerLed;
    QLabel *outProt1Led;
    QLabel *outProt2Led;
    QLabel *tempProtLed;
    QLabel *invErrLed;
    QLabel *phaseErrLed;

    QPushButton *startBtn;
    QPushButton *stopBtn;

    QLineSeries *currentSeries;
    QChart *currentChart;
    float time_axis;
    float updateFreq;

    QVBoxLayout *logLayout;
    QWidget *logContainer;
    QListWidget *logFileList;
};

#endif // QTUI_H