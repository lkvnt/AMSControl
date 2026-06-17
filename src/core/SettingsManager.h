#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QString>
#include <QVariantMap>
#include <QVariant>

class SettingsManager {
public:
    static SettingsManager& instance();

    void load();
    void save();
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant());
    void set(const QString& key, const QVariant& value);

private:
    SettingsManager(); // Приватный конструктор для Синглтона
    QVariantMap m_settings;

    void createDefault();
};

#endif // SETTINGSMANAGER_H