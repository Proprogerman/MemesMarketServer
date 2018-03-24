#include "myserver.h"
#include "serverrunnable.h"
#include "updatingdata.h"

MyServer::MyServer(QObject* parent):
    QTcpServer(parent), upd(new UpdatingData())
{
    threadPool = new QThreadPool(this);
    threadPool->reserveThread();
    if(listen(QHostAddress::Any, 1234)){
        qDebug()<<"Listening...";
    }
    else{
        qDebug()<<"Error while starting!";
    }
    threadPool->setMaxThreadCount(10);
}

void MyServer::incomingConnection(qintptr handle)
{
    ServerRunnable *runnable = new ServerRunnable();
    runnable->setDescriptor(handle);
    threadPool->start(runnable);
}
