#ifndef SERVERRUNNABLE_H
#define SERVERRUNNABLE_H

#include "cscommunication.h"

#include <QRunnable>
#include <QTcpSocket>
#include <QDebug>
#include <QTime>
#include <QEventLoop>
#include <QThread>
#include <QByteArray>

class ServerRunnable : public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit ServerRunnable(QObject* parent = 0);
    ~ServerRunnable();

    void setDescriptor(qintptr desc);
    void run();
    quint32 arrayToInt(QByteArray dataSize);
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

#endif //SERVERRUNNABLE_H
