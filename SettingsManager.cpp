#include "SettingsManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

SettingsManager& SettingsManager::instance() {
    static SettingsManager inst;
    return inst;
}

SettingsManager::SettingsManager() { 
    load(); 
}

void SettingsManager::load() {
    QFile file("settings.json");
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        m_settings = doc.object().toVariantMap();
    } else {
        createDefault();
    }
}

void SettingsManager::save() {
    QFile file("settings.json");
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(QJsonObject::fromVariantMap(m_settings));
        file.write(doc.toJson());
    }
}

QVariant SettingsManager::get(const QString& key, const QVariant& defaultValue) {
    return m_settings.value(key, defaultValue);
}

void SettingsManager::set(const QString& key, const QVariant& value) {
    m_settings[key] = value;
}

void SettingsManager::createDefault() {
    m_settings["update_frequency"] = 10;
    m_settings["power_deviceId"] = 0x16;
    m_settings["cool_mockTemp"] = 24.5;
    m_settings["cool_deviceId"] = 0x3C;
    m_settings["sensor_deviceId"] = 0x3E;
    m_settings["sensor_chanFaraday1"] = 0x10;
    m_settings["sensor_chanHall"] = 0x11;
    m_settings["sensor_chanFaraday2"] = 0x12;
    m_settings["sensor_chanVacuum"] = 0x13;
    m_settings["log_intervalMs"] = 500;
    m_settings["can_bus_poll_timer"] = 10;
    m_settings["interface_freshness_limit"] = 5000;
    save();
}