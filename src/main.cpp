#include "mainwindow.h"

#include <QApplication>
#include <QTextCodec>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setOrganizationName("sonnayasomnambula");
    a.setOrganizationDomain("sonnayasomnambula.github.io");
    a.setApplicationName("Geoviever");
    a.setApplicationVersion("0.3");

    MainWindow w;
    w.show();

    return a.exec();
}
