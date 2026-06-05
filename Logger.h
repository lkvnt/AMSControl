#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QVariant>
#include <QRunnable>
#include <QThreadPool>

class Logger {
public:
    virtual ~Logger() = default;
    
    virtual void log(const QString& dirPath, const QString& fileName, const QVariant& data) = 0;
};

#endif // LOGGER_H