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
    mngr(new QNetworkAccessManager()), timer(new QTimer()),
    creativityTimer(new QTimer())
{
    connect(timer, &QTimer::timeout, this, &UpdatingData::onTimerTriggered);
    connect(creativityTimer, &QTimer::timeout, [this](){ updateUsersCreativity(); });
    connect(creativityTimer, &QTimer::timeout, [this](){ updateUsersShekels(); });
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
        timer->start(10000);
        creativityTimer->start(60000);
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
    if(query.exec("SELECT id, vk_id FROM memes;")){

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
    else
        database.open();
}

void UpdatingData::updateMemesPopValues(QJsonArray arr){
    QSqlQuery query(database);
    if(query.exec("SELECT id, pop_values, edited_by_user FROM memes;")){
        QSqlRecord rec = query.record();
        query.first();

        for(int i = 0; i < arr.size(); i++){
            QJsonObject obj = arr[i].toObject();
            int likes = obj.value("likes").toObject().value("count").toInt();
            int views = obj.value("views").toObject().value("count").toInt();
            int reposts = obj.value("reposts").toObject().value("count").toInt();
            //double popValue = (likes + reposts * 5.0)/ views;
            int popValue = qrand() % 201 - 100;                        //пределы значения популярности от -100 до 100

            int memeId = query.value(rec.indexOf("id")).toInt();
            bool editedByUser = query.value(rec.indexOf("edited_by_user")).toBool();
            if(!editedByUser){
                QJsonArray popArr = QJsonDocument::fromJson(query.value(rec.indexOf("pop_values")).toByteArray()).array();
                if(popArr.size() < memesPopValuesCount){
                    popArr.append(popValue);
                }
                else if(popArr.size() == memesPopValuesCount){
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
            }
            else{
                QSqlQuery updateQuery;
                updateQuery.exec(QString("UPDATE memes SET edited_by_user = 0 WHERE id = '%1';")
                                 .arg(memeId));
            }
            query.next();
        }
        updateUsersPopValues();
    }
    else
        database.open();
}

void UpdatingData::getVkResponse(QNetworkReply *reply){
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray arr = doc.object().value("response").toArray();
    updateMemesPopValues(arr);
}

void UpdatingData::updateUsersPopValues(){
    QSqlQuery namesQuery(database);
    if(namesQuery.exec("SELECT name FROM users")){
        while(namesQuery.next()){
            QString name = namesQuery.value(0).toString();
            QSqlQuery updateQuery(database);
            updateQuery.exec(QString("SELECT users.name, memes.name, pop_values, weight, startPopValue, pop_value FROM memes "
                            "INNER JOIN user_memes ON memes.id = meme_id "
                            "INNER JOIN category_weight ON memes.category = category_weight.category "
                            "INNER JOIN users ON users.id = user_id WHERE users.name = '%1';")
                            .arg(name));
            QSqlRecord rec = updateQuery.record();

            int memePopValuesIndex = rec.indexOf("pop_values");
            int weightIndex = rec.indexOf("weight");
            int userPopValueIndex = rec.indexOf("pop_value");


            int popValueChange = 0;
            while(updateQuery.next()){
                QJsonArray popArr = QJsonDocument::fromJson(updateQuery.value(memePopValuesIndex).toByteArray()).array();
                popValueChange += static_cast<int>(popArr.last().toInt() * updateQuery.value(weightIndex).toDouble());
            }
            if(updateQuery.size() != 0){
                QSqlQuery query(database);
                updateQuery.last();
                int userPopValue = updateQuery.value(userPopValueIndex).toInt();
                //qDebug()<<"userPopValue: "<<updateQuery.value(userPopValueIndex).toInt();
                if(userPopValue + popValueChange > 0)
                    query.exec(QString("UPDATE users SET pop_value = pop_value + %1 WHERE name = '%2';").arg(popValueChange).arg(name));
                else
                    query.exec(QString("UPDATE users SET pop_value = 0 WHERE name = '%1';").arg(name));
                qDebug()<<"name: "<< name;
                qDebug()<<"popValueChange: " << popValueChange;
            }
        }
    }
    else
        database.open();
}

void UpdatingData::updateUsersCreativity()
{
    QSqlQuery creativityQuery(database);
    if(creativityQuery.exec("SELECT name, creativity FROM users;")){
        while(creativityQuery.next()){
            QSqlRecord rec = creativityQuery.record();
            QString name = creativityQuery.value(rec.indexOf("name")).toString();
            int creativity = creativityQuery.value(rec.indexOf("creativity")).toInt();
            int creativityIncrement = 20;                                       // может быть индивидуальным
            QSqlQuery updateQuery(database);
            if(creativity + creativityIncrement <= 100){
                updateQuery.exec(QString("UPDATE users SET creativity = creativity + '%1' WHERE name = '%2';")
                                 .arg(creativityIncrement)
                                 .arg(name));
            }
            else{
                updateQuery.exec(QString("UPDATE users SET creativity = 100 WHERE name = '%1';")
                                 .arg(name));
            }
        }
    }
    else
        database.open();
}

void UpdatingData::updateUsersShekels()
{
    QSqlQuery shekelsQuery(database);
    if(shekelsQuery.exec("SELECT name, shekels FROM users;")){
        QSqlRecord rec = shekelsQuery.record();
        int nameIndex = rec.indexOf("name");
        int shekelsIndex = rec.indexOf("shekels");
        while(shekelsQuery.next()){
            QString name = shekelsQuery.value(nameIndex).toString();
            int shekels = shekelsQuery.value(shekelsIndex).toInt();
            int shekelsIncrement = 50;                                   // может быть индивидуальным
            QSqlQuery updateQuery(database);
            updateQuery.exec(QString("UPDATE users SET shekels = shekels + '%1' WHERE name = '%2' AND online = 1;")
                                    .arg(shekelsIncrement)
                                    .arg(name));
        }
    }
    else
        database.open();
}
