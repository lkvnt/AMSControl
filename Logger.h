#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QVariant>
#include <QRunnable>
#include <QThreadPool>

class Logger {
public:
    virtual ~Logger() = default;
    
    // Менеджер передает только директорию, имя файла и сырые данные
    virtual void log(const QString& dirPath, const QString& fileName, const QVariant& data) = 0;
};

class LogTask : public QRunnable {
private:
    Logger* m_logger;
    QString m_dirPath;
    QString m_fileName;
    QVariant m_data;
public:
    LogTask(Logger* logger, const QString& dirPath, const QString& fileName, const QVariant& data)
        : m_logger(logger), m_dirPath(dirPath), m_fileName(fileName), m_data(data) {
        setAutoDelete(true); 
    }

    void run() override {
        if (m_logger) {
            m_logger->log(m_dirPath, m_fileName, m_data);
        }
    }
};



#endif // LOGGER_H