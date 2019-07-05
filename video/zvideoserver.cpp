#include "zvideoserver.h"
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <QDebug>
#include <zgblpara.h>
ZVideoThread::ZVideoThread(int socketDescriptor,QObject *parent):QThread(parent)
{
    this->m_sockFd=socketDescriptor;
    this->m_bSpsSended=false;

    for(qint32 i=0;i<FIFO_DEPTH;i++)
    {
        this->m_fifo[i]=new ZVideoPkt;
        this->m_fifo[i]->m_baData=new QByteArray;
        this->m_fifo[i]->m_baData->resize(FIFO_SIZE);
        this->m_fifo[i]->m_nDataLen=0;
        this->m_freeQueueIn.enqueue(this->m_fifo[i]);
    }
    this->m_usedQueueIn.clear();
}
ZVideoThread::~ZVideoThread()
{
    for(qint32 i=0;i<FIFO_DEPTH;i++)
    {
        this->m_fifo[i]->m_baData->resize(0);
        delete this->m_fifo[i]->m_baData;
        delete this->m_fifo[i];
    }
}
void ZVideoThread::run()
{
    QTcpSocket *socket=new QTcpSocket;
    if(!socket->setSocketDescriptor(this->m_sockFd))
    {
        qDebug()<<"<error>:failed to create new video thread!";
        delete socket;
        emit this->ZSigFinished();
        return;
    }

    while(!gGblPara.m_bGblRst2Exit && socket->state()==QAbstractSocket::ConnectedState)
    {
        //1.get a valid buffer from fifo.
        bool bTryAgain=false;
        this->m_mutexIn.lock();
        while(this->m_usedQueueIn.isEmpty())
        {
            if(!this->m_condQueueFullIn.wait(&this->m_mutexIn,5000))//timeout 5s to check exit flag.
            {
                this->m_mutexIn.unlock();
                bTryAgain=true;
                break;
            }
        }
        if(bTryAgain)
        {
            continue;
        }

        ZVideoPkt *baH264Pkt=this->m_usedQueueIn.dequeue();

        //tx sps/pps to client.
        if(false==this->m_bSpsSended)
        {
            //send sps/pps to client.
            QByteArray baSpsPpsLen=qint32ToQByteArray(gGblPara.m_video.m_nSpsPpsLen);
            socket->write(baSpsPpsLen);
            socket->write((const char*)gGblPara.m_video.m_pSpsPspBuffer,gGblPara.m_video.m_nSpsPpsLen);
            socket->waitForBytesWritten(1000);

            //set flag.
            this->m_bSpsSended=true;
        }
        QByteArray baH264PktLen=qint32ToQByteArray(baH264Pkt->m_nDataLen);
        if(socket->write(baH264PktLen)!=baH264PktLen.size())
        {
            qDebug()<<"<Error>:failed to tx h264 pkt len,break socket.";
        }
        if(socket->write((const char*)baH264Pkt->m_baData->data(),baH264Pkt->m_nDataLen)!=baH264Pkt->m_nDataLen)
        {
            qDebug()<<"<Error>:failed to tx h264 data,break socket.";
        }
        if(!socket->waitForBytesWritten(SOCKET_TX_TIMEOUT))//1s.
        {
            qDebug()<<"tx failed:"<<socket->errorString();
        }
        //qDebug()<<this->m_sockFd<<"tx one h264 okay";

        this->m_freeQueueIn.enqueue(baH264Pkt);
        this->m_condQueueEmptyIn.wakeAll();
        this->m_mutexIn.unlock();
    }

    socket->close();
    delete socket;
    emit this->ZSigFinished();
    return;
}
ZVideoServer::ZVideoServer(qint32 nListenPort,QMutex *mutex)
{
    this->m_nListenPort=nListenPort;
    this->m_mutex=mutex;
}
ZVideoServer::~ZVideoServer()
{
}
qint32 ZVideoServer::ZStartServer()
{
    int on=1;
    int sockFd=this->socketDescriptor();
    setsockopt(sockFd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
    if(!this->listen(QHostAddress::Any,this->m_nListenPort))
    {
        qDebug()<<"<Error>:tcp server error listen on port"<<this->m_nListenPort;
        return -1;
    }
    return 0;
}
void ZVideoServer::incomingConnection(int socketDescriptor)
{
    ZVideoThread *thread=new ZVideoThread(socketDescriptor);
    QObject::connect(thread,SIGNAL(ZSigFinished()),this,SLOT(ZSlotDisconnected()));

    //add to list.
    this->m_mutex->lock();
    this->m_threadList.append(thread);
    this->m_mutex->unlock();

    //start thread.
    thread->start();

    //set connected flag to notify h264EncTx thread to work.
    gGblPara.m_bVideoTcpConnected=true;

    qDebug()<<"one video client connected.";

    return;
}
void ZVideoServer::ZSlotDisconnected()
{
    ZVideoThread *thread=qobject_cast<ZVideoThread*>(this->sender());
    if(thread==NULL)
    {
        return;
    }

    this->m_mutex->lock();
    this->m_threadList.removeOne(thread);
    if(this->m_threadList.size()==0)
    {
        //set connected flag to notify h264EncTx thread to stop.
        gGblPara.m_bVideoTcpConnected=false;
    }
    this->m_mutex->unlock();

    thread->quit();
    thread->wait(1000);
    delete thread;
    thread=NULL;
    qDebug()<<"one video client disconnected.";
}
