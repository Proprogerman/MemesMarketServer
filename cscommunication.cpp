#include "cscommunication.h"

#include <QDebug>
#include <QVariant>
#include <QList>
#include <QVector>

#include <QJsonArray>

CSCommunication::CSCommunication(QTcpSocket *tcpSocket, QObject *parent) : respSock(tcpSocket),
    QObject(parent)
{
    connectToDatabase();

    connect(this, &CSCommunication::nameAvailable, this, &CSCommunication::onNameAvailable);
}

void CSCommunication::connectToDatabase(){
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

void CSCommunication::processingRequest(QJsonObject &jsonObj){
    qDebug()<<"processingRequest()";

    QString requestType = jsonObj["requestType"].toString();
    qDebug()<<requestType;

    if(requestType == "checkName"){
        qDebug()<<"requestType == nameCheck";
        checkName(jsonObj["user_name"].toString());
    }
    else if(requestType == "signUp"){
        qDebug()<<"requestType == signUp";
        signUp(jsonObj);
    }
    else if(requestType == "getMemeList"){
        qDebug()<<"requestType == getMemeList";
        getMemeList(jsonObj);
    }
}

void CSCommunication::onNameAvailable(bool val){
    QJsonObject jsonObj{
        { "responseType", "checkNameResponse" },
        { "nameAvailable", val }
    };

    respSock->write(QJsonDocument(jsonObj).toBinaryData());
    respSock->waitForBytesWritten(3000);
}

void CSCommunication::checkName(const QString &name){
    QSqlQuery query(database);
    query.prepare("SELECT * FROM users WHERE name = :name;");
    query.bindValue(":name", name);

    query.exec();

    if(query.size() == 0)
    {
        qDebug()<<"name " << name << " available";
        emit nameAvailable(true);
    }
    else{
        qDebug()<<"name " << name << "not available";
        emit nameAvailable(false);
    }
}

void CSCommunication::signUp(QJsonObject &jsonObj){
    QSqlQuery query(database);
    query.prepare("INSERT INTO users (name, password) VALUES(:name, :password);");
    query.bindValue(":name", jsonObj["user_name"].toString());
    query.bindValue(":password", jsonObj["user_password"].toString());
    qDebug()<<"signUp(): \n"<<"name: "<<jsonObj["user_name"].toString()<<"\n"<<jsonObj["user_password"].toString()<<endl;
    query.exec();
}

void CSCommunication::getMemeList(QJsonObject &jsonObj)
{
    qDebug()<<"USER_NAME" <<jsonObj["use_name"].toString();
    QSqlQuery query(database);
//    query.prepare("SELECT memes.name, memes.pop_values, memes.image FROM users "
//                  "INNER JOIN user_memes ON users.id = user_id "
//                  "INNER JOIN memes ON meme_id = memes.id WHERE users.name = :user_name;");
//    query.bindValue(":user_name", "KingOfMemes");
//    query.exec();
    query.exec( QString("SELECT memes.name, memes.pop_values, memes.image FROM users "
                        "INNER JOIN user_memes ON users.id = user_id "
                        "INNER JOIN memes ON meme_id = memes.id WHERE users.name = '%1';").arg("KingOfMemes"));
    qDebug()<<"getMemeList()";
    QSqlRecord rec = query.record();
    qDebug()<<"query record:"<<rec;
    int memeNameIndex = rec.indexOf("name");
    int memePopIndex = rec.indexOf("pop_values");

    qDebug() << "QUERY SIZE= " << query.size();

    QVariantList memeList;

    while(query.next()){
        QVariantMap memeObj;
        memeObj.insert("memeName", query.value(memeNameIndex).toString());
        memeObj.insert("popValues", QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array());
        qDebug()<<"POP VALUES: "<< memeObj.value("popValues");
        memeList << memeObj;
        qDebug() << "MEMEOBJ: " << memeObj;
        memeObj.clear();
    }

    QJsonObject jsonMemeList;
    jsonMemeList.insert("responseType", "getMemeListResponse");
    jsonMemeList.insert("memeList", QJsonArray::fromVariantList(memeList));
    respSock->write(QJsonDocument(jsonMemeList).toBinaryData());
    qDebug()<<"jsonMemeList: "<<jsonMemeList;
    respSock->waitForBytesWritten(3000);
}


