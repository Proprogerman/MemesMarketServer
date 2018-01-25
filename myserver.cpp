#include "myserver.h"
#include "qserverrunnable.h"
#include "updatingdata.h"

MyServer::MyServer(QObject* parent):
    QTcpServer(parent), upd(new UpdatingData())
{
    if(listen(QHostAddress::Any, 1234)){
        qDebug()<<"Listening...";
    }
    else{
        qDebug()<<"Error while starting!";
    }
    threadPool = new QThreadPool(this);
    //threadPool->setMaxThreadCount(20);
}

void MyServer::incomingConnection(qintptr handle)
{
    QServerRunnable *runnable = new QServerRunnable();
    runnable->setDescriptor(handle);
    threadPool->start(runnable);
}