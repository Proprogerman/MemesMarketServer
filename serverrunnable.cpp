#include "serverrunnable.h"
#include "cscommunication.h"


ServerRunnable::ServerRunnable(QObject* parent): QObject(parent)
{

}

ServerRunnable::~ServerRunnable()
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

    m_socket->setSocketDescriptor(m_descriptor);
    m_comm = new CSCommunication(m_socket);

    connect(m_socket, &QTcpSocket::readyRead, this, &ServerRunnable::onReadyRead, Qt::DirectConnection);
    connect(m_socket, &QTcpSocket::disconnected, this, &ServerRunnable::onDisconnected, Qt::DirectConnection);

    m_eventLoop->exec();
    delete m_socket;
    delete m_eventLoop;
}

quint32 ServerRunnable::arrayToInt(QByteArray dataSize)
{
    quint32 temp;
    QDataStream stream(&dataSize, QIODevice::ReadWrite);
    stream >> temp;
    return temp;
}

void ServerRunnable::onReadyRead()
{
    while(m_socket->bytesAvailable()){
        QByteArray byteArr;

        quint32 dataSize = arrayToInt(m_socket->read(sizeof(quint32)));
        byteArr = m_socket->read(dataSize);

        while(byteArr.size() < dataSize){
            m_socket->waitForReadyRead();
            byteArr.append(m_socket->read(dataSize - byteArr.size()));
        }

        QJsonObject jsonObj = QJsonDocument::fromBinaryData(byteArr).object();

        m_comm->processingRequest(jsonObj);
    }
}

void ServerRunnable::onDisconnected()
{
    delete m_comm;
    m_eventLoop->exit();
}
