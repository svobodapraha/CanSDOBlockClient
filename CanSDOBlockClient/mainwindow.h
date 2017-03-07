#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCanBus>
#include "CanSDOBlockClient.h"
#include <stdint.h>
#include <QDebug>


#define KN_MAIN_TIMER_INTERVAL  100

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    int fnSendCanMessage(XMC_LMOCan_t *CanMessageToSend);

private slots:
    void on_BtnInitBus_clicked();
    void fnFramesReceived();


    void on_BtnNewFile_clicked();

public slots:
    void on_BtnSend_clicked();


private:
    Ui::MainWindow *ui;
    QCanBusDevice *device;
    void timerEvent(QTimerEvent *event);
    int iMainTimerId;
};

extern MainWindow *pMainWindow;

#endif // MAINWINDOW_H
