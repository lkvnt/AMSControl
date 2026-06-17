#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QChart>
#include "src/core/SettingsManager.h"

namespace Theme {

inline QString getDarkStylesheet() {
    return R"(
        QMainWindow, QDialog { background-color: #121212; }
        QWidget { font-family: 'Segoe UI', 'Roboto', sans-serif; font-size: 13px; color: #e0e0e0; }
        QTabWidget::pane { border: 1px solid #333; border-radius: 6px; background: #1e1e1e; top: -1px; }
        QTabBar::tab { background: #2a2a2a; color: #aaa; padding: 8px 16px; border: 1px solid #333; border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; }
        QTabBar::tab:selected { background: #1e1e1e; color: #fff; border: 1px solid #333; border-bottom: 1px solid #1e1e1e; font-weight: bold; }
        QTabBar::tab:hover:!selected { background: #333; color: #fff; }
        QGroupBox { border: 1px solid #444; border-radius: 6px; margin-top: 20px; background-color: #1e1e1e; padding-top: 15px; }
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; top: -10px; padding: 2px 8px; color: #2196F3; font-weight: bold; font-size: 14px; background-color: #2a2a2a; border-radius: 4px; border: 1px solid #444; }
        QSpinBox, QDoubleSpinBox { background: #2a2a2a; color: #ffffff; border: 1px solid #444; padding: 5px; border-radius: 4px; min-height: 18px; }
        QSpinBox::up-button, QDoubleSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::down-button { width: 16px; background: #333; border-left: 1px solid #444; }
        QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover, QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background: #2196F3; }
        QScrollBar:vertical, QScrollBar:horizontal { background: #121212; width: 12px; height: 12px; margin: 0px; }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #444; min-height: 20px; min-width: 20px; border-radius: 6px; margin: 2px; }
        QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background: #555; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0px; width: 0px; }
        QPushButton { border: 1px solid #444; border-radius: 6px; padding: 6px 12px; background-color: #2a2a2a; color: #e0e0e0; font-weight: bold; }
        QPushButton:hover:enabled { background-color: #383838; border: 1px solid #2196F3; }
        QPushButton:pressed { background-color: #2196F3; color: white; border: 1px solid #1976D2; }
        QPushButton:disabled { background-color: #1e1e1e; color: #666; border: 1px solid #333; }
        QComboBox { border: 1px solid #444; border-radius: 6px; padding: 5px 10px; background-color: #2a2a2a; color: #e0e0e0; min-height: 20px; }
        QComboBox:hover { border: 1px solid #2196F3; }
        QComboBox::drop-down { border-left: 1px solid #444; width: 20px; }
        QComboBox QAbstractItemView { background-color: #2a2a2a; color: #e0e0e0; selection-background-color: #2196F3; border: 1px solid #444; outline: none; }
    )";
}

inline QString getLightStylesheet() {
    return R"(
        QMainWindow, QDialog { background-color: #f0f0f0; }
        QWidget { font-family: 'Segoe UI', 'Roboto', sans-serif; font-size: 13px; color: #111; }
        QTabWidget::pane { border: 1px solid #c5c5c5; border-radius: 6px; background: #ffffff; top: -1px; }
        QTabBar::tab { background: #e1e1e1; color: #555; padding: 8px 16px; border: 1px solid #c5c5c5; border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; }
        QTabBar::tab:selected { background: #ffffff; color: #000; border: 1px solid #c5c5c5; border-bottom: 1px solid #ffffff; font-weight: bold; }
        QTabBar::tab:hover:!selected { background: #dcdcdc; color: #000; }
        QGroupBox { border: 1px solid #c5c5c5; border-radius: 6px; margin-top: 20px; background-color: #fdfdfd; padding-top: 15px; }
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; top: -10px; padding: 2px 8px; color: #005cc5; font-weight: bold; font-size: 14px; background-color: #e1e1e1; border-radius: 4px; border: 1px solid #c5c5c5; }
        QSpinBox, QDoubleSpinBox { background: #ffffff; color: #111; border: 1px solid #c5c5c5; padding: 5px; border-radius: 4px; min-height: 18px; }
        QSpinBox::up-button, QDoubleSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::down-button { width: 16px; background: #e1e1e1; border-left: 1px solid #c5c5c5; }
        QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover, QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background: #007bff; }
        QScrollBar:vertical, QScrollBar:horizontal { background: #f0f0f0; width: 12px; height: 12px; margin: 0px; }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #c5c5c5; min-height: 20px; min-width: 20px; border-radius: 6px; margin: 2px; }
        QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background: #b0b0b0; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0px; width: 0px; }
        QPushButton { border: 1px solid #c5c5c5; border-radius: 6px; padding: 6px 12px; background-color: #e1e1e1; color: #111; font-weight: bold; }
        QPushButton:hover:enabled { background-color: #dcdcdc; border: 1px solid #007bff; }
        QPushButton:pressed { background-color: #007bff; color: white; border: 1px solid #0056b3; }
        QPushButton:disabled { background-color: #f5f5f5; color: #aaa; border: 1px solid #ddd; }
        QComboBox { border: 1px solid #c5c5c5; border-radius: 6px; padding: 5px 10px; background-color: #e1e1e1; color: #111; min-height: 20px; }
        QComboBox:hover { border: 1px solid #007bff; }
        QComboBox::drop-down { border-left: 1px solid #c5c5c5; width: 20px; }
        QComboBox QAbstractItemView { background-color: #ffffff; color: #111; selection-background-color: #007bff; border: 1px solid #c5c5c5; outline: none; }
    )";
}

inline QString getAppStylesheet() {
    QString theme = SettingsManager::instance().get("theme", "dark").toString();
    if (theme == "light") {
        return getLightStylesheet();
    }
    return getDarkStylesheet();
}

} // namespace Theme

#endif // THEME_H