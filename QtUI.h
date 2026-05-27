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

class SystemManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(SystemManager *manager, QWidget *parent = nullptr);
    ~MainWindow() = default;

private slots:
    void onTimerTick();          
    void handleSetCurrent();     
    void handleStart();
    void handleStop();
    void onLogMessage(const QString& msg);
    void onLogFileDoubleClicked(QListWidgetItem *item);
    void refreshLogList();
    void onBusyStateChanged(bool isBusy);
    void onDataFileDoubleClicked(QListWidgetItem *item);

private:
    void setupUI();
    void updateLamps(uint8_t status);
    
    SystemManager *m_manager;
    QTimer *updateTimer;

    QLabel *tempLabel;
    QLabel *flowLabel;
    QDoubleSpinBox *currentSpinBox;
    QPushButton *setBtn;
    QPushButton *mainSettingsBtn;
    QPushButton *powSettingsBtn;
    QPushButton *coolSettingsBtn;
    QPushButton *measSettingsBtn;
    QPushButton *logSettingsBtn;
    QLabel *faradayLabel;
    QLabel *hallLabel;
    QLabel *vacuumVoltLabel;
    QLabel *vacuumPressLabel;

    QLabel *currentValLabel;
    QLabel *adcVoltLabel;
    
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
    QLabel *pumpLed;
    QLabel *radiatorLed;

    QPushButton *startBtn;
    QPushButton *stopBtn;

    QLineSeries *currentSeries;
    QChart *currentChart;
    float time_axis;
    float updateFreq;

    QVBoxLayout *logLayout;
    QWidget *logContainer;
    QListWidget *logFileList;
    QListWidget *dataFileList;
};

#endif // QTUI_H