#ifndef LOGWORKER_H
#define LOGWORKER_H

#include <QObject>
#include <QVariant>
#include <memory>
#include "Logger.h"

class LogWorker : public QObject {
    Q_OBJECT
public:
    LogWorker(std::unique_ptr<Logger> eventLogger, 
              std::unique_ptr<Logger> telemetryLogger, 
              QObject* parent = nullptr);

public slots:
    void onEventLogRequested(const QString& dirPath, const QString& fileName, const QVariant& data);

    void onTelemetryLogRequested(const QString& dirPath, const QString& fileName, const QVariant& data);

private:
    std::unique_ptr<Logger> m_eventLogger;
    std::unique_ptr<Logger> m_telemetryLogger;
};

#endif // LOGWORKER_H