#include "mainwindow.h"
#include <QSystemTrayIcon>
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <unistd.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);


    MainWindow w;
    w.show();
    return a.exec();
}
