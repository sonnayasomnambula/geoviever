#include "mainwindow.h"

#include <QApplication>
#include <QTextCodec>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setOrganizationName("sonnayasomnambula");
    a.setOrganizationDomain("sonnayasomnambula.github.io");
    a.setApplicationName("geoviever");
    a.setApplicationVersion("0.2");

    MainWindow w;
    w.show();

    return a.exec();
}
