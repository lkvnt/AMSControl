#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QString>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const QString& tabName, QWidget *parent = nullptr);

signals:
    void reqPowerOn();
    void reqPowerOff();
    void reqResetProt();
    void reqSetCurrent(float current);
    void reqCoolingOn();
    void reqCoolingOff();
};

#endif // SETTINGSDIALOG_H