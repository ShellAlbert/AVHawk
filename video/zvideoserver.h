#ifndef ZVIDEOLISTENSERVER_H
#define ZVIDEOLISTENSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <zgblpara.h>
class ZVideoPkt
{
public:
    QByteArray *m_baData;
    qint32 m_nDataLen;
};
class ZVideoThread:public QThread
{
    Q_OBJECT
public:
    ZVideoThread(int socketDescriptor,QObject *parent = Q_NULLPTR);
    ~ZVideoThread();
    bool m_bSpsSended;
public:
    ZVideoPkt* m_fifo[FIFO_DEPTH];
    QQueue<ZVideoPkt*> m_freeQueueIn;
    QQueue<ZVideoPkt*> m_usedQueueIn;
    QMutex m_mutexIn;
    QWaitCondition m_condQueueEmptyIn;
    QWaitCondition m_condQueueFullIn;
signals:
    void ZSigFinished();
protected:
    void run();
private:
    int m_sockFd;
};

class ZVideoServer : public QTcpServer
{
    Q_OBJECT
public:
    ZVideoServer(qint32 nListenPort,QMutex *mutex);
    ~ZVideoServer();
public:
    QList<ZVideoThread*> m_threadList;
public:
    qint32 ZStartServer();
    qint32 ZStopServer();
private slots:
    void ZSlotDisconnected();
protected:
    void incomingConnection(int socketDescriptor);
private:
    qint32 m_nListenPort;
    QMutex *m_mutex;
};

#endif // ZVIDEOLISTENSERVER_H
