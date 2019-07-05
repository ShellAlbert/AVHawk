#ifndef ZAUDIOTXTHREAD_H
#define ZAUDIOTXTHREAD_H

#include <QTcpSocket>
#include <QTcpServer>
#include <QThread>
#include <QMap>
#include <QQueue>
#include <QSemaphore>
#include <QWaitCondition>
#include<audio/zaudioserver.h>
class ZAudioEncThread:public QThread
{
    Q_OBJECT
public:
    ZAudioEncThread(QList<ZAudioThread*> *threadList,QMutex *mutex);
    void ZBindFIFO(QQueue<QByteArray*> *freeQueue,QQueue<QByteArray*> *usedQueue,///<
                   QMutex *mutex,QWaitCondition *condQueueEmpty,QWaitCondition *condQueueFull);
    qint32 ZStartThread();
    qint32 ZStopThread();
    bool ZIsExitCleanup();
signals:
    void ZSigOpusReady(const QByteArray &baOpus);
    void ZSigThreadFinished();
protected:
    void run();
private:
    bool m_bCleanup;
private:
    QQueue<QByteArray*> *m_freeQueue;
    QQueue<QByteArray*> *m_usedQueue;

    QMutex *m_mutex;
    QWaitCondition *m_condQueueEmpty;
    QWaitCondition *m_condQueueFull;

private:
    QList<ZAudioThread*> *m_threadList;
    QMutex *m_mutexThread;
};
#endif // ZAUDIOTXTHREAD_H
