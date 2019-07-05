#include "zaudioserver.h"
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <QDebug>
#include <zgblpara.h>
ZAudioThread::ZAudioThread(int socketDescriptor,QObject *parent):QThread(parent)
{
    this->m_sockFd=socketDescriptor;

    for(qint32 i=0;i<5;i++)
    {
        this->m_fifo[i]=new ZAudioPkt;
        this->m_fifo[i]->m_baData=new QByteArray;
        this->m_fifo[i]->m_baData->resize(PERIOD_SIZE);
        this->m_fifo[i]->m_nDataLen=0;
        this->m_freeQueueIn.enqueue(this->m_fifo[i]);
    }
    this->m_usedQueueIn.clear();
}
ZAudioThread::~ZAudioThread()
{
    for(qint32 i=0;i<5;i++)
    {
        this->m_fifo[i]->m_baData->resize(0);
        delete this->m_fifo[i]->m_baData;
        delete this->m_fifo[i];
    }
}
void ZAudioThread::run()
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

        ZAudioPkt *baOpusPkt=this->m_usedQueueIn.dequeue();
        QByteArray baOpusPktLen=qint32ToQByteArray(baOpusPkt->m_nDataLen);
        if(socket->write(baOpusPktLen)!=baOpusPktLen.size())
        {
            qDebug()<<"<Error>:failed to tx opus pkt len,break socket.";
        }
        if(socket->write((const char*)baOpusPkt->m_baData->data(),baOpusPkt->m_nDataLen)!=baOpusPkt->m_nDataLen)
        {
            qDebug()<<"<Error>:failed to tx opus data,break socket.";
        }
        if(!socket->waitForBytesWritten(SOCKET_TX_TIMEOUT))//1s.
        {
            qDebug()<<"tx failed:"<<socket->errorString();
        }
        //qDebug()<<this->m_sockFd<<"tx one opus okay";

        this->m_freeQueueIn.enqueue(baOpusPkt);
        this->m_condQueueEmptyIn.wakeAll();
        this->m_mutexIn.unlock();
    }

    socket->close();
    delete socket;
    emit this->ZSigFinished();
    return;
}

ZAudioServer::ZAudioServer(qint32 nListenPort,QMutex *mutex)
{
    this->m_nListenPort=nListenPort;
    this->m_mutex=mutex;
}
ZAudioServer::~ZAudioServer()
{

}
qint32 ZAudioServer::ZStartServer()
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
void ZAudioServer::incomingConnection(int socketDescriptor)
{
    ZAudioThread *thread=new ZAudioThread(socketDescriptor);
    QObject::connect(thread,SIGNAL(ZSigFinished()),this,SLOT(ZSlotDisconnected()));

    //add to list.
    this->m_mutex->lock();
    this->m_threadList.append(thread);
    this->m_mutex->unlock();

    //start thread.
    thread->start();

    //set connected flag to notify AudioCap/opusEnc thread to work.
    gGblPara.m_audio.m_bAudioTcpConnected=true;

    qDebug()<<"one audio client connected.";;
    return;
}
void ZAudioServer::ZSlotDisconnected()
{
    ZAudioThread *thread=qobject_cast<ZAudioThread*>(this->sender());
    if(thread==NULL)
    {
        return;
    }

    this->m_mutex->lock();
    this->m_threadList.removeOne(thread);
    if(this->m_threadList.size()==0)
    {
        //set connected flag to notify opus thread to stop.
        gGblPara.m_audio.m_bAudioTcpConnected=false;
    }
    this->m_mutex->unlock();

    thread->quit();
    thread->wait(1000);
    delete thread;
    thread=NULL;
    qDebug()<<"one audio client disconnected.";
}
