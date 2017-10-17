#include "qserverrunnable.h"
#include "cscommunication.h"

QServerRunnable::QServerRunnable(QObject* parent): QObject(parent)
{

}

void QServerRunnable::setDescriptor(qintptr desc)
{
    m_descriptor = desc;
}

void QServerRunnable::run()
{
    m_eventLoop = new QEventLoop();
    m_socket = new QTcpSocket();
    m_comm = new CSCommunication(m_socket);

    m_socket->setSocketDescriptor(m_descriptor);
    qDebug()<<"socketDescriptor: "<<m_descriptor;

    connect(m_socket, &QTcpSocket::readyRead, this, &QServerRunnable::onReadyRead, Qt::DirectConnection);
    connect(m_socket, &QTcpSocket::disconnected, this, &QServerRunnable::onDisconnected, Qt::DirectConnection);

    m_eventLoop->exec();
    delete m_socket;
    delete m_eventLoop;
}

void QServerRunnable::onReadyRead()
{
    qDebug()<<"onReadyRead()";

    QByteArray byteArr = m_socket->readAll();

    QJsonObject jsonObj = QJsonDocument::fromBinaryData(byteArr).object();

    m_comm->processingRequest(jsonObj);
    //QThread::sleep(5);
}

void QServerRunnable::onDisconnected()
{
    qDebug()<<"onDiconnected()";
    m_eventLoop->exit();
}

