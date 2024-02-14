#include "mainwindow.h"

#include <QApplication>
#include <QTextCodec>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setOrganizationName("sonnayasomnambula");
    a.setOrganizationDomain("sonnayasomnambula.github.io");
    a.setApplicationName("geoviever");
    a.setApplicationVersion("0.1");

#ifdef Q_OS_WINDOWS
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("CP 1251"));
#endif

    MainWindow w;
    w.show();

    return a.exec();
}
