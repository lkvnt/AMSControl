#include <QApplication>
#include "QtUI.h"

int main(int argc, char *argv[])
{
    // 1. Создаем объект приложения. 
    // Он управляет ресурсами системы и очередью сообщений.
    QApplication app(argc, argv);

    // Установка метаданных (полезно для настроек и логирования)
    QApplication::setApplicationName("Power & Cooling Control System");
    QApplication::setApplicationVersion("1.0.0");

    // 2. Создаем экземпляр вашего главного окна.
    // Внутри конструктора MainWindow должна быть логика 
    // инициализации SystemManager и вкладок.
    MainWindow window;

    // 3. Отображаем окно на экране
    window.show();

    // 4. Запускаем цикл обработки событий. 
    // Программа будет работать, пока вы не закроете окно.
    return app.exec();
}