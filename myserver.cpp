#include "myserver.h"
#include "serverrunnable.h"
#include "updatingdata.h"


MyServer::MyServer(QObject* parent):
    QTcpServer(parent), upd(new UpdatingData())
{
    threadPool = new QThreadPool(this);

    QFile fil(":/authData.json");
    fil.open(QFile::ReadOnly | QIODevice::Text);
    QString val = fil.readAll();
    fil.close();
    QJsonObject obj = QJsonDocument::fromJson(val.simplified().toUtf8()).object().value("Server").toObject();

    if(listen(QHostAddress::Any, obj["port"].toString().toInt()))
        qDebug()<<"Listening...";
    else
        qDebug()<<"Error while starting!";

    threadPool->setMaxThreadCount(10);
}

MyServer::~MyServer(){
    delete threadPool;
    delete upd;
}

void MyServer::incomingConnection(qintptr handle)
{
    ServerRunnable *runnable = new ServerRunnable();
    runnable->setAutoDelete(true);
    runnable->setDescriptor(handle);
    threadPool->start(runnable);
}
