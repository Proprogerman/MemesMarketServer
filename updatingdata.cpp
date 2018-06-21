#include "updatingdata.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>

#include <QtMath>

#include <QUrl>

#include <QDateTime>


UpdatingData::UpdatingData(QObject *parent) : QObject(parent),
    mngr(new QNetworkAccessManager()), timer(new QTimer()),
    creativityTimer(new QTimer())
{
    connectToDatabase();
    connect(timer, &QTimer::timeout, this, &UpdatingData::onTimerTriggered);
    connect(creativityTimer, &QTimer::timeout, [this](){ updateUsersCreativity(); });
    connect(timer, &QTimer::timeout, [this](){ updateUserAdTime(); });

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
    QString postsRequestString;
    QString wallRequestString;
    postsRequestString.append("https://api.vk.com/method/wall.getById?posts=");
    while(memesIter.hasNext()){
        memesIter.next();
        wallRequestString.append("https://api.vk.com/method/wall.get?owner_id=");
        postsRequestString.append(memesIter.value());
        int idSize = 0;
        for(int i = 0; memesIter.value().at(i) != '_'; i++){
            idSize = i;
        }
        wallRequestString.append(memesIter.value().mid(0, idSize + 1));
        wallRequestString.append("&access_token=094b40d2094b40d2094b40d29e0917b1eb0094b094b40d2501aa9b9710314fdf82f8efd"
                                 "&count=1&v=5.74");
        QNetworkReply *wallReply = mngr->get(QNetworkRequest(QUrl(wallRequestString)));
        connect(wallReply, &QNetworkReply::finished, [=](){setPostsCount(wallReply);});
        if(memesIter.hasNext())
            postsRequestString.append(",");
    }
    postsRequestString.append("&access_token=094b40d2094b40d2094b40d29e0917b1eb0094b094b40d2501aa9b9710314fdf82f8efd"
                              "&extended=1&fields=members_count&v=5.74");

    QNetworkReply *postsReply = mngr->get(QNetworkRequest(QUrl(postsRequestString)));
    connect(postsReply, &QNetworkReply::finished, [=](){updateMemesPopValues(postsReply);});
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

void UpdatingData::updateMemesPopValues(QNetworkReply *reply){
    QSqlQuery query(database);
    if(query.exec("SELECT id, pop_values, edited_by_user FROM memes;")){
        QSqlRecord rec = query.record();
        query.first();
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object().value("response").toObject();
        QJsonArray items = obj.value("items").toArray();
        QJsonArray groups = obj.value("groups").toArray();

        for(int i = 0; i < items.size(); i++){
            QJsonObject item = items[i].toObject();
            int likes = item.value("likes").toObject().value("count").toInt();
            int reposts = item.value("reposts").toObject().value("count").toInt();
            int comments = item.value("comments").toObject().value("count").toInt();
            int views = item.value("views").toObject().value("count").toInt();
            int ownerId = item.value("owner_id").toInt();
            int groupIndex = 0;

            for(int i = 0; i < groups.size(); i++){
                if(groups[i].toObject().value("owner_id").toInt() == ownerId)
                    groupIndex = i;
            }
            int members = groups[groupIndex].toObject().value("members_count").toInt();

            unsigned int activity = views * likes * comments * reposts;
            int popValue = qCeil(activity * 1.0 / members);

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
    qDebug()<<"_________________________________________";
}

void UpdatingData::setPostsCount(QNetworkReply *reply)
{
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object().value("response").toObject();
    int postCount = obj.value("count").toInt();
    int ownerId = obj.value("items").toArray().first().toObject().value("owner_id").toInt();
    postsCount.insert(ownerId, postCount);
}

void UpdatingData::updateUsersPopValues(){
    QSqlQuery namesQuery(database);
    if(namesQuery.exec("SELECT name FROM users")){
        while(namesQuery.next()){
            QString name = namesQuery.value(0).toString();
            QSqlQuery updateQuery(database);
            updateQuery.exec(QString("SELECT users.name, memes.name, pop_values, feedbackRate, startPopValue, pop_value, "
                                     "user_memes.creativity FROM memes "
                                     "INNER JOIN user_memes ON memes.id = meme_id "
                                     "INNER JOIN users ON users.id = user_id WHERE users.name = '%1';")
                                     .arg(name));
            QSqlRecord rec = updateQuery.record();

            int memePopValuesIndex = rec.indexOf("pop_values");
//            int weightIndex = rec.indexOf("weight");
//            int feedbackRateIndex = rec.indexOf("feedbackRate");
            int userPopValueIndex = rec.indexOf("pop_value");
            int startPopValueIndex = rec.indexOf("startPopValue");
            int creativityIndex = rec.indexOf("creativity");

            int popValueChange = 0;
            while(updateQuery.next()){
                QJsonArray popArr = QJsonDocument::fromJson(updateQuery.value(memePopValuesIndex).toByteArray()).array();
//                popValueChange += static_cast<int>(popArr.last().toInt() * updateQuery.value(weightIndex).toDouble());
                int currentValue = popArr.last().toInt();
                int startPopValue = updateQuery.value(startPopValueIndex).toInt();
//                double feedbackrate = updateQuery.value(feedbackRateIndex).toDouble();
                double creativityRate = updateQuery.value(creativityIndex).toDouble() / 100;
                popValueChange += static_cast<int>(currentValue * (1 + creativityRate) - startPopValue);
//                popValueChange += static_cast<int>(popArr.last().toInt() * updateQuery.value(feedbackRateIndex).toDouble());
            }
            if(updateQuery.size() != 0){
                QSqlQuery query(database);
                updateQuery.last();
                int userPopValue = updateQuery.value(userPopValueIndex).toInt();
                //qDebug()<<"userPopValue: "<<updateQuery.value(userPopValueIndex).toInt();
                if(userPopValue + popValueChange > 0){
                    query.exec(QString("UPDATE users SET pop_value = pop_value + %1 WHERE name = '%2';")
                               .arg(popValueChange)
                               .arg(name));
                }
                else
                    query.exec(QString("UPDATE users SET pop_value = 0 WHERE name = '%1';").arg(name));
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

void UpdatingData::updateUserAdTime()
{
    QSqlQuery userAdQuery(database);
    if(userAdQuery.exec("SELECT user_id, ad_id, unavailableUntil FROM user_ad;")){
        QSqlRecord rec = userAdQuery.record();
        int userIdIndex = rec.indexOf("user_id");
        int adIdIndex = rec.indexOf("ad_id");
        int unavailableUntilIndex = rec.indexOf("unavailableUntil");
        while(userAdQuery.next()){
            QDateTime dateTimeOfUserAd = userAdQuery.value(unavailableUntilIndex).toDateTime();
            if(dateTimeOfUserAd <= QDateTime::currentDateTime()){
                QSqlQuery deleteQuery(database);
                QString deleteQueryString = QString("DELETE FROM user_ad WHERE user_id = '%1' AND ad_id = '%2';")
                                                    .arg(userAdQuery.value(userIdIndex).toInt())
                                                    .arg(userAdQuery.value(adIdIndex).toInt());
                deleteQuery.exec(deleteQueryString);
            }
        }
    }
    else
        database.open();
}
