#ifndef ZAUDIOSERVER_H
#define ZAUDIOSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QObject>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <QQueue>
#include <zgblpara.h>
class ZAudioPkt
{
public:
    QByteArray *m_baData;
    qint32 m_nDataLen;
};
class ZAudioThread:public QThread
{
    Q_OBJECT
public:
    ZAudioThread(int socketDescriptor,QObject *parent = Q_NULLPTR);
    ~ZAudioThread();
public:
    ZAudioPkt* m_fifo[5];
    QQueue<ZAudioPkt*> m_freeQueueIn;
    QQueue<ZAudioPkt*> m_usedQueueIn;
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
class ZAudioServer : public QTcpServer
{
    Q_OBJECT
public:
    ZAudioServer(qint32 nListenPort,QMutex *mutex);
    ~ZAudioServer();

    qint32 ZStartServer();
    qint32 ZStopServer();

    QList<ZAudioThread*> m_threadList;
private slots:
    void ZSlotDisconnected();
protected:
    void incomingConnection(int socketDescriptor);
private:
    qint32 m_nListenPort;
    QMutex *m_mutex;
};

#endif // ZAUDIOSERVER_H
