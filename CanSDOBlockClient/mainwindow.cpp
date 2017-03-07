

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>




int fnSendCanMessage(XMC_LMOCan_t *CanMessageToSend)
{

    pMainWindow->fnSendCanMessage(CanMessageToSend);
    return(0);
}

int MainWindow::fnSendCanMessage(XMC_LMOCan_t *CanMessageToSend)
{
   qDebug() << "Sending";
   QCanBusFrame FrameToSend;
   FrameToSend.setFrameType(QCanBusFrame::DataFrame);
   FrameToSend.setFrameId(CanMessageToSend->can_identifier);
   QByteArray MessagePayload;
   MessagePayload.clear();
   MessagePayload.append((char *)CanMessageToSend->can_data, CanMessageToSend->can_data_length);
   FrameToSend.setPayload(MessagePayload);
   device->writeFrame(FrameToSend);
   return(0);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("CanSDOBlockClient");
    iMainTimerId = startTimer(KN_MAIN_TIMER_INTERVAL);
}


void MainWindow::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == iMainTimerId)
    {
        fnClientTimer(KN_MAIN_TIMER_INTERVAL);
    }

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_BtnSend_clicked()
{
    eCommStatus = comclst_InitiateBlockDownload;
    fnRunStateMachine(NULL, 0);
}


void MainWindow::on_BtnNewFile_clicked()
{
      QString asNewFirmwareFileName = QFileDialog::getOpenFileName
              (
                  this,
                  "Choose New Firmware file",
                  "..",
                  "*.crcbin"
              );
      if(!asNewFirmwareFileName.isEmpty())
      {
          NewFirmwareFileName = (char *) malloc(asNewFirmwareFileName.length() + 1);
          strcpy((char *)NewFirmwareFileName, asNewFirmwareFileName.toStdString().c_str() );
      }
      else
      {
          NewFirmwareFileName = NULL;
      }
}


void MainWindow::on_BtnInitBus_clicked()
{
    foreach (const QByteArray &backend, QCanBus::instance()->plugins())
    {
        if (backend == "socketcan")
        {
            qDebug() << backend;
            break;
        }
    }

    device = QCanBus::instance()->createDevice("socketcan", QStringLiteral("vcan0"));
    qDebug() << device;
    qDebug() << device->connectDevice();

    QObject::connect(device, SIGNAL( framesReceived()),
                       this, SLOT(fnFramesReceived()));


    fnInit();

}

void MainWindow::fnFramesReceived()
{
    while(device->framesAvailable() > 0)
    {
        qDebug() << "Client SLOT fnFramesReceived, avaiable" << device->framesAvailable();
        QCanBusFrame ReceivedFrame = device->readFrame();
        qDebug() << "Main:" << QString::number(ReceivedFrame.frameId(),16).toUpper () << "#" << ReceivedFrame.payload().toHex().toUpper();
        qDebug() << "***";

        if (ReceivedFrame.frameType() == QCanBusFrame::DataFrame)
        {
            XMC_LMOCan_t ReceivedCanMsg;
            memset(&ReceivedCanMsg,0, sizeof(XMC_LMOCan_t));
            ReceivedCanMsg.can_identifier  = ReceivedFrame.frameId();
            ReceivedCanMsg.can_data_length = ReceivedFrame.payload().size();
            int u8SizeToCopy = 0;
            if (ReceivedFrame.payload().size() < (signed)sizeof(ReceivedCanMsg.can_data))
                u8SizeToCopy = ReceivedFrame.payload().size();
            else
                u8SizeToCopy = sizeof(ReceivedCanMsg.can_data);
            memcpy(ReceivedCanMsg.can_data, ReceivedFrame.payload().data(), u8SizeToCopy);
            fnProcessCANMessage(&ReceivedCanMsg);
            qDebug() << "sizes:" << sizeof(ReceivedCanMsg.can_data) << sizeof(XMC_LMOCan_t::can_data);
        }

    }
}

