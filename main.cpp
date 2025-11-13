#include "MainWindow.h"
#include <QApplication>
#include <iostream>
#include <QDebug.h>
#include <QDir>
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // =============================================================================
    // УСТАНОВКА ПУТЕЙ ПРИЛОЖЕНИЯ
    // =============================================================================
    // Убеждаемся, что рабочая директория - это папка с исполняемым файлом
    QDir::setCurrent(QApplication::applicationDirPath());

    MainWindow window;
    window.show();
    return app.exec();
}