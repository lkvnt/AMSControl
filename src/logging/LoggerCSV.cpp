#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QVariantMap>
#include "src/logging/LoggerCSV.h"

LoggerCSV::LoggerCSV(const QStringList& headers) : m_headers(headers) {}

void LoggerCSV::log(const QString& dirPath, const QString& fileName, const QVariant& data) {
    QString fullFileName = fileName + ".csv";
    QDir dir;
    if (!dir.exists(dirPath)) {
        dir.mkpath(dirPath);
    }

    QFile file(dirPath + "/" + fullFileName);
    bool isNew = !file.exists();

    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);

        if (isNew && !m_headers.isEmpty()) {
            out << m_headers.join(",") << "\n";
        }

        QVariantMap dataMap = data.toMap();
        QStringList row;

        for (const QString& header : m_headers) {
            row.append(dataMap.value(header, "").toString());
        }

        out << row.join(",") << "\n";
        file.close();
    }
}