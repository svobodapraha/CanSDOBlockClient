#include "mainwindow.h"
#include <QApplication>

MainWindow *pMainWindow = NULL;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    pMainWindow = &w;
    w.show();

    return a.exec();
}
