#ifndef MYSERVER_H
#define MYSERVER_H

#include<QTcpServer>
#include<QThreadPool>

#include "updatingdata.h"

class MyServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit MyServer(QObject *parent = 0);
    ~MyServer();
    void incomingConnection(qintptr handle);

private:
    QThreadPool *threadPool;
    UpdatingData *upd;
};

#endif // MYSERVER_H
