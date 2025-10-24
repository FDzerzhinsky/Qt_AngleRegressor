#include "MainWindow.h"
#include <QApplication>
#include <iostream>
#include <QDebug.h>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.show();
    return app.exec();
}