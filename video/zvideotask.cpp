#include "zvideotask.h"
#include <video/zfiltecamdev.h>
#include <QStringList>
#include <QDebug>
#include <QDateTime>
#include <QPainter>
#include <QApplication>
#include <QDir>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>
#include <unistd.h>
#include <linux/videodev2.h>
ZVideoTask::ZVideoTask(QObject *parent) : QObject(parent)
{
    this->m_capThread=NULL;
    this->m_encThread=NULL;
    this->m_timerExit=new QTimer;
    QObject::connect(this->m_timerExit,SIGNAL(timeout()),this,SLOT(ZSlotChkAllExitFlags()));
}
ZVideoTask::~ZVideoTask()
{
    delete this->m_timerExit;
    delete this->m_capThread;
    delete this->m_encThread;

    //clean FIFOs.
    //main capture to h264 encoder queue(fifo).
    for(qint32 i=0;i<FIFO_DEPTH;i++)
    {
        delete this->m_Cap2EncFIFOMain[i];
    }
    this->m_Cap2EncFIFOFreeMain.clear();
    this->m_Cap2EncFIFOUsedMain.clear();
}
qint32 ZVideoTask::ZDoInit()
{
    //create FIFOs.
    //main capture to h264 encoder queue(fifo).
    for(qint32 i=0;i<FIFO_DEPTH;i++)
    {
        this->m_Cap2EncFIFOMain[i]=new QByteArray;
        this->m_Cap2EncFIFOMain[i]->resize(FIFO_SIZE);
        this->m_Cap2EncFIFOFreeMain.enqueue(this->m_Cap2EncFIFOMain[i]);
    }
    this->m_Cap2EncFIFOUsedMain.clear();


    //create capture thread.
    this->m_capThread=new ZImgCapThread("/dev/video0",720,480,15);//Main Camera.

    this->m_capThread->ZBindOutFIFO(&this->m_Cap2EncFIFOFreeMain,&this->m_Cap2EncFIFOUsedMain,&this->m_Cap2EncFIFOMutexMain,&this->m_condCap2EncFIFOEmptyMain,&this->m_condCap2EncFIFOFullMain);
    QObject::connect(this->m_capThread,SIGNAL(ZSigFinished()),this,SLOT(ZSlotSubThreadsFinished()));

    //create multiple connect server.
    this->m_videoServer=new ZVideoServer(TCP_PORT_VIDEO,&this->m_mutex);

    //create h264 enc thread.
    this->m_encThread=new ZHardEncThread(&this->m_videoServer->m_threadList,&this->m_mutex);
    this->m_encThread->ZBindInFIFO(&this->m_Cap2EncFIFOFreeMain,&this->m_Cap2EncFIFOUsedMain,&this->m_Cap2EncFIFOMutexMain,&this->m_condCap2EncFIFOEmptyMain,&this->m_condCap2EncFIFOFullMain);
    QObject::connect(this->m_encThread,SIGNAL(ZSigFinished()),this,SLOT(ZSlotSubThreadsFinished()));

    return 0;
}
ZImgCapThread* ZVideoTask::ZGetImgCapThread(qint32 index)
{
    return this->m_capThread;
}
qint32 ZVideoTask::ZStartTask()
{
    if(this->m_videoServer->ZStartServer()<0)
    {
        qDebug()<<"[VideoServer]:failed to start video server.";
        return -1;
    }
    this->m_encThread->ZStartThread();
    this->m_capThread->ZStartThread();
    return 0;
}
void ZVideoTask::ZSlotSubThreadsFinished()
{
    if(!this->m_timerExit->isActive())
    {
        //notify all working threads to exit.
        this->m_capThread->ZStopThread();
        this->m_encThread->ZStopThread();
        //start timer to help unblocking the queue empty or full.
        this->m_timerExit->start(1000);
    }
}
void ZVideoTask::ZSlotChkAllExitFlags()
{
    if(gGblPara.m_bGblRst2Exit)
    {
        //if CapThread[0] doesnot exit,maybe m_Cap2ProFIFOFreeMain is empty to cause thread blocks.
        //or m_Cap2EncFIFOFreeMain is empty to cause thread blocks.
        //here we move a element from m_Cap2ProFIFOUsedMain to m_Cap2ProFIFOFreeMain to unblock.
        if(!this->m_capThread->ZIsExitCleanup())
        {
            qDebug()<<"<Exiting>:waiting for MainVideoCaptureThread...";
            this->m_Cap2EncFIFOMutexMain.lock();
            if(!this->m_Cap2EncFIFOUsedMain.isEmpty())
            {
                QByteArray *elementHelp=this->m_Cap2EncFIFOUsedMain.dequeue();
                this->m_Cap2EncFIFOFreeMain.enqueue(elementHelp);
                this->m_condCap2EncFIFOEmptyMain.wakeAll();
            }
            this->m_Cap2EncFIFOMutexMain.unlock();
        }


        //if m_encTxThread doesnot exit,maybe m_Cap2EncFIFOFreeMain is empty to cause thread blocks.
        //here we move a element from m_Cap2EncFIFOUsedMain to m_Cap2EncFIFOFreeMain to unblock.
        if(!this->m_encThread->ZIsExitCleanup())
        {
            qDebug()<<"<Exiting>:waiting for MainEncTxThread...";
            this->m_Cap2EncFIFOMutexMain.lock();
            if(!this->m_Cap2EncFIFOUsedMain.isEmpty())
            {
                QByteArray *elementHelp=this->m_Cap2EncFIFOUsedMain.dequeue();
                this->m_Cap2EncFIFOFreeMain.enqueue(elementHelp);
                this->m_condCap2EncFIFOEmptyMain.wakeAll();
            }
            this->m_Cap2EncFIFOMutexMain.unlock();
        }

        //video task exit after all sub threads exited.
        if(this->ZIsExitCleanup())
        {
            this->m_timerExit->stop();
            emit this->ZSigVideoTaskExited();
        }
    }
}
bool ZVideoTask::ZIsExitCleanup()
{
    bool bCleanup=true;
    if(!this->m_capThread->ZIsExitCleanup())
    {
        bCleanup=false;
    }
    if(!this->m_encThread->ZIsExitCleanup())
    {
        bCleanup=false;
    }
    return bCleanup;
}
