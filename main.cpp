#include <QApplication>
#include "QtUI.h"

int main(int argc, char *argv[])
{
    // Настройка атрибутов для корректного отображения графиков на High DPI мониторах
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);

    // Установка имени приложения (полезно для настроек и заголовков)
    a.setApplicationName("Power Supply Control System (VCH-300)");
    a.setApplicationVersion("1.0.0");

    MainWindow w;
    w.show();

    return a.exec();
}