#ifndef MYSERVER_H
#define MYSERVER_H

#include<QTcpServer>
#include<QThreadPool>

class MyServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit MyServer(QObject *parent = 0);
    void incomingConnection(qintptr handle);

private:
    QThreadPool *threadPool;
};

#endif // MYSERVER_H
