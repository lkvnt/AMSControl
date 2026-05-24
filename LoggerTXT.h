#ifndef LOGGERTXT_H
#define LOGGERTXT_H

#include "Logger.h"

class LoggerTXT : public Logger {
public:
    void log(const QString& dirPath, const QString& fileName, const QVariant& data) override;
};

#endif // LOGGERTXT_H