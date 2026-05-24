#ifndef LOGGERCSV_H
#define LOGGERCSV_H

#include "Logger.h"
#include <QStringList>

class LoggerCSV : public Logger {
public:
    explicit LoggerCSV(const QStringList& headers);
    void log(const QString& dirPath, const QString& fileName, const QVariant& data) override;

private:
    QStringList m_headers;
};

#endif // LOGGERCSV_H