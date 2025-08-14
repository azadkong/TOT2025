#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName("GFC Editor");
    QApplication::setOrganizationName("ExampleOrg");
    QApplication::setApplicationDisplayName("GFC Editor");

    MainWindow w;
    w.resize(1200, 800);
    w.show();
    return a.exec();
}
