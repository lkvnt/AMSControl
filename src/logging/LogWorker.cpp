#include "src/logging/LogWorker.h"

LogWorker::LogWorker(std::unique_ptr<Logger> eventLogger, 
              std::unique_ptr<Logger> telemetryLogger, 
              QObject* parent)
        : QObject(parent), 
          m_eventLogger(std::move(eventLogger)), 
          m_telemetryLogger(std::move(telemetryLogger)) {}


void LogWorker::onEventLogRequested(const QString& dirPath, const QString& fileName, const QVariant& data) {
    if (m_eventLogger) {
        m_eventLogger->log(dirPath, fileName, data);
    }
}

void LogWorker::onTelemetryLogRequested(const QString& dirPath, const QString& fileName, const QVariant& data) {
    if (m_telemetryLogger) {
        m_telemetryLogger->log(dirPath, fileName, data);
    }
}
