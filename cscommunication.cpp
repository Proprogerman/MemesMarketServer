#include "cscommunication.h"

#include<QDebug>

CSCommunication::CSCommunication(QTcpSocket *tcpSocket, QObject *parent) : respSock(tcpSocket),
    QObject(parent)
{
    connectToDatabase();

    connect(this, &CSCommunication::nameAvailable, this, &CSCommunication::onNameAvailable);
}

void CSCommunication::connectToDatabase()
{
    database = QSqlDatabase::addDatabase("QMYSQL");
    database.setHostName("127.0.0.1");
    database.setDatabaseName("appschema");
    database.setPort(3306);
    database.setUserName("root");
    database.setPassword("root");
    if(!database.open())
        qDebug()<<"database is not open!";
    else
        qDebug()<<"database is open...";
}

void CSCommunication::processingRequest(QJsonObject &jsonObj)
{
//    switch(jsonObj["requestType"]){
//        case "signUp":
//        {
//            signUp(jsonObj);
//            break;
//        }
//        case "nameCheck":
//        {
//            checkName(jsonObj["user_name"]);
//            break;
//        }
//    }
    QString requestType = jsonObj["requestType"].toString();
    qDebug()<<requestType;

    qDebug()<<"processingRequest()";

    if(requestType == "signUp"){
        qDebug()<<"requestType == signUp";
        signUp(jsonObj);
    }
    else if(requestType == "checkName"){
        qDebug()<<"requestType == nameCheck";
        checkName(jsonObj["user_name"].toString());
    }
}

void CSCommunication::onNameAvailable(bool val)
{
    QJsonObject jsonObj{
        { "responseType", "checkNameResponse" },
        { "nameAvailability", val }
    };

    respSock->write(QJsonDocument(jsonObj).toBinaryData());
    respSock->waitForBytesWritten(3000);
}


void CSCommunication::checkName(const QString &name)
{
    QSqlQuery query(database);
    query.prepare("SELECT * FROM users WHERE name = :name;");
    query.bindValue(":name", name);

    query.exec();

    if(query.size() == 0)
    {
        user_name = name;
        qDebug()<<"name " << user_name << " available";
        emit nameAvailable(true);
    }
    else{
        qDebug()<<"name " << user_name << "not available";
        emit nameAvailable(false);
    }
}

void CSCommunication::setPassword(const QString &password){
    user_password = password;
}

//void CSCommunication::setToken(const QString &token)
//{
//    //загрузка токена из БД!!!!
//}

void CSCommunication::signUp(QJsonObject &jsonObj)
{
    QSqlQuery query(database);
    query.prepare("INSERT INTO users (name, password) VALUES(:name, :password);");
    query.bindValue(":name", jsonObj["user_name"].toString());
    query.bindValue(":password", jsonObj["user_password"].toString());
    qDebug()<<"signUp(): \n"<<"name: "<<jsonObj["user_name"].toString()<<"\n"<<jsonObj["user_password"].toString()<<endl;
    query.exec();
}

QString CSCommunication::getName(){
    return user_name;
}

QString CSCommunication::getPassword(){
    return user_password;
}

QString CSCommunication::getUserToken()
{
    return user_token;
}


