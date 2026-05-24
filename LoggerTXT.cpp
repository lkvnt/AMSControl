#include "LoggerTXT.h"
#include <QDir>
#include <QFile>
#include <QTextStream>

void LoggerTXT::log(const QString& dirPath, const QString& fileName, const QVariant& data) {
    QString fullFileName = fileName + ".txt";
    QDir dir;
    if (!dir.exists(dirPath)) {
        dir.mkpath(dirPath);
    }

    QFile file(dirPath + "/" + fullFileName);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << data.toString() << "\n";
        file.close();
    }
}