#include "updatingdata.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QNetworkRequest>
#include <QNetworkReply>

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>

#include <QUrl>


UpdatingData::UpdatingData(QObject *parent) : QObject(parent),
    mngr(new QNetworkAccessManager()), timer(new QTimer())
{
    connectToDatabase();
    connect(mngr, &QNetworkAccessManager::finished, [=](QNetworkReply *reply){getVkResponse(reply);});

    qDebug()<<"UpdatingData constructor";
}

void UpdatingData::connectToDatabase(){
    database = QSqlDatabase::addDatabase("QMYSQL");
    database.setHostName("127.0.0.1");
    database.setDatabaseName("appschema");
    database.setPort(3306);
    database.setUserName("root");
    database.setPassword("root");
    if(!database.open())
        qDebug()<<"database is not open!";
    else{
        qDebug()<<"database is open...";
        connect(timer, &QTimer::timeout, this, &UpdatingData::onTimerTriggered);
        timer->start(10000);
    }
}

void UpdatingData::vkApi(){
    QMapIterator<int, QString> memesIter(memesMap);
    QString apiRequest;
    apiRequest.append("https://api.vk.com/method/wall.getById?posts=");
    while(memesIter.hasNext()){
        memesIter.next();
        apiRequest.append("-" + memesIter.value());
        if(memesIter.hasNext())
            apiRequest.append(",");
    }
    apiRequest.append("&v=5.69");

    mngr->get(QNetworkRequest(QUrl(apiRequest)));
}

void UpdatingData::onTimerTriggered(){
    QSqlQuery query(database);
    query.exec("SELECT id, vk_id FROM memes;");

    QSqlRecord rec = query.record();
    int idIndex = rec.indexOf("id");
    int vk_idIndex = rec.indexOf("vk_id");

    while(query.next()){
        memesMap.insert(query.value(idIndex).toInt(), query.value(vk_idIndex).toString());
    }
    if(!query.next()){
        vkApi();
    }
}

void UpdatingData::updatePop(QJsonArray arr){
    QSqlQuery query(database);
    query.exec("SELECT id, pop_values FROM memes;");
    QSqlRecord rec = query.record();
    query.first();

    for(int i = 0; i < arr.size(); i++){
        QJsonObject obj = arr[i].toObject();
        int likes = obj.value("likes").toObject().value("count").toInt();
        int views = obj.value("views").toObject().value("count").toInt();
        int reposts = obj.value("reposts").toObject().value("count").toInt();
        double popValue = (likes + reposts * 5.0)/ views;

        int memeId = query.value(rec.indexOf("id")).toInt();
        QJsonArray popArr = QJsonDocument::fromJson(query.value(rec.indexOf("pop_values")).toByteArray()).array();
        if(popArr.size() < 6){                                    //magic number: 6- ограничение количества значений
            popArr.append(popValue);
        }
        else if(popArr.size() == 6){
            for(int i = 0; i < popArr.size() - 1; i++){
                popArr[i] = popArr[i + 1];
            }
            popArr[popArr.size() - 1] = popValue;
        }
        QSqlQuery updateQuery;
        QJsonDocument doc;
        doc.setArray(popArr);
        updateQuery.exec(QString("UPDATE memes SET pop_values='%1' WHERE id='%2';")
                         .arg(QString(doc.toJson(QJsonDocument::Compact)))
                         .arg(memeId));
        qDebug()<<"LAST QUERY: "<<updateQuery.lastQuery();
        query.next();
    }
}

void UpdatingData::getVkResponse(QNetworkReply *reply){
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray arr = doc.object().value("response").toArray();
    updatePop(arr);
}
