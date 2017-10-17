#ifndef QSERVERRUNNABLE_H
#define QSERVERRUNNABLE_H

#include "cscommunication.h"

#include<QRunnable>
#include<QTcpSocket>
#include<QDebug>
#include<QTime>
#include<QEventLoop>
#include<QThread>

class QServerRunnable : public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit QServerRunnable(QObject* parent = 0);

    void setDescriptor(qintptr desc);
    void run();
private:
    CSCommunication* m_comm;
    qintptr m_descriptor;
    QTcpSocket* m_socket;
    QEventLoop* m_eventLoop;
signals:

public slots:
    void onReadyRead();
    void onDisconnected();
};

#endif //QSERVERRUNNABLE_H
