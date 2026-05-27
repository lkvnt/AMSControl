#include <QApplication>
#include "QtUI.h"
#include "Manager.h"
#include "LoggerCSV.h"
#include "LoggerTXT.h"

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);

    a.setApplicationName("Accelerator Mass-Spectrometer Control");
    a.setApplicationVersion("1.0.0");

    auto txtLogger = std::make_unique<LoggerTXT>();
    QStringList csvHeaders = {"timestamp", "current", "temp", "flow", "hall", "ioncurrent", "pressure"};
    auto csvLogger = std::make_unique<LoggerCSV>(csvHeaders);
    
    SystemManager manager(std::move(txtLogger), std::move(csvLogger));

    MainWindow w(&manager);
    w.show();

    return a.exec();
}