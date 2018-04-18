#include "serverrunnable.h"
#include "cscommunication.h"

ServerRunnable::ServerRunnable(QObject* parent): QObject(parent)
{

}

void ServerRunnable::setDescriptor(qintptr desc)
{
    m_descriptor = desc;
}

void ServerRunnable::run()
{
    m_eventLoop = new QEventLoop();
    m_socket = new QTcpSocket();
    m_comm = new CSCommunication(m_socket);

    m_socket->setSocketDescriptor(m_descriptor);
    qDebug()<<"socketDescriptor: "<<m_descriptor;

    connect(m_socket, &QTcpSocket::readyRead, this, &ServerRunnable::onReadyRead, Qt::DirectConnection);
    connect(m_socket, &QTcpSocket::disconnected, this, &ServerRunnable::onDisconnected, Qt::DirectConnection);

    m_eventLoop->exec();
    delete m_socket;
    delete m_eventLoop;
}

void ServerRunnable::onReadyRead()
{
    qDebug()<<"onReadyRead()";

    QByteArray byteArr = m_socket->readAll();

    QJsonObject jsonObj = QJsonDocument::fromBinaryData(byteArr).object();

    m_comm->processingRequest(jsonObj);
    //QThread::sleep(5);
}

void ServerRunnable::onDisconnected()
{
    qDebug()<<"onDiconnected()";
    m_eventLoop->exit();
    delete m_comm;
}

