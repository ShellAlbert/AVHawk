#ifndef ZVIDEOTXTHREADH264_H
#define ZVIDEOTXTHREADH264_H

#include <QThread>
#include <QQueue>
#include <QSemaphore>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWaitCondition>
#include <QMutex>
#include <video/zvideoserver.h>
class ZHardEncThread : public QThread
{
    Q_OBJECT
public:
    ZHardEncThread(QList<ZVideoThread*> *threadList,QMutex *mutex);
    //main capture -> encTxThread.
    void ZBindInFIFO(QQueue<QByteArray*> *freeQueue,QQueue<QByteArray*> *usedQueue,///<
                   QMutex *mutex,QWaitCondition *condQueueEmpty,QWaitCondition *condQueueFull);
    qint32 ZStartThread();
    qint32 ZStopThread();
    bool ZIsExitCleanup();
private:
    void ZDoCleanBeforeExit();
signals:
    void ZSigFinished();

protected:
    void run();
private:
    bool m_bCleanup;
private:
    //in fifo.(main capture -> encTxThread).
    QQueue<QByteArray*> *m_freeQueueIn;
    QQueue<QByteArray*> *m_usedQueueIn;
    QMutex *m_mutexIn;
    QWaitCondition *m_condQueueEmptyIn;
    QWaitCondition *m_condQueueFullIn;
private:
    QList<ZVideoThread*> *m_threadList;
    QMutex *m_mutex;
};

#endif // ZVIDEOTXTHREADH264_H
