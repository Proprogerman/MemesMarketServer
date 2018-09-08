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
    setAuthData();
    connectToDatabase();
    connect(timer, &QTimer::timeout, this, &UpdatingData::onTimerTriggered);
    connect(creativityTimer, &QTimer::timeout, [this](){ updateUsersCreativity(); });
    connect(timer, &QTimer::timeout, [this](){ updateUserAdTime(); });
    connect(timer, &QTimer::timeout, [this](){ updateMemeLoyalty(); });
}

UpdatingData::~UpdatingData(){
    delete mngr;
    delete timer;
    delete creativityTimer;
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
    int postsCounter = 0;
    while(memesIter.hasNext()){
        memesIter.next();
        postsCounter++;
        if(postsRequestString.isEmpty())
            postsRequestString = "https://api.vk.com/method/wall.getById?posts=";
        postsRequestString.append(memesIter.value());
        int idSize = 0;
        while(idSize < memesIter.value().size() - 1){
            if(memesIter.value().at(idSize) == '_')
                break;
            ++idSize;
        }
        if(memesIter.hasNext() && postsCounter < 100)
            postsRequestString.append(",");

        if(!memesIter.hasNext() || postsCounter == 100){
            postsRequestString.append("&extended=1&fields=members_count&v=5.80&access_token=");
            postsRequestString.append(accessToken);
            QNetworkReply *postsReply = mngr->get(QNetworkRequest(QUrl(postsRequestString)));
            connect(postsReply, &QNetworkReply::finished, [=](){updateMemesPopValues(postsReply);});
            postsCounter = 0;
            postsRequestString.clear();
        }
    }
}

void UpdatingData::onTimerTriggered(){
    QString checkMemesRequestString = "https://api.vk.com/method/photos.get?owner_id=-170377182&v=5.80&album_id=255910742&"
                                      "access_token=";
    checkMemesRequestString.append(accessToken);
    QNetworkReply *checkMemesReply = mngr->get(QNetworkRequest(QUrl(checkMemesRequestString)));
    connect(checkMemesReply, &QNetworkReply::finished, [=](){checkMemesFromHub(checkMemesReply);});

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

void UpdatingData::checkMemesFromHub(QNetworkReply *reply)
{
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object()["response"].toObject();
    QJsonArray items = obj["items"].toArray();
    for(int i = 0; i < items.size(); i++){
        QJsonObject item = items[i].toObject();
        QJsonObject memeData = QJsonDocument::fromJson(item["text"].toString().simplified().toUtf8()).object();
        QSqlQuery query(database);
        QString checkMemeInDatabaseString = QString("SELECT id FROM memes WHERE name = '%1';")
                                                    .arg(memeData["name"].toString());
        query.exec(checkMemeInDatabaseString);
        if(!query.size()){
            QJsonArray images = item["sizes"].toArray();
            QSqlQuery insertMemeQuery(database);
            QString insertMemeQueryString = QString("INSERT INTO memes (name,image,pop_values,vk_id,category) "
                                              "VALUES('%1','%2','[0]','%3','%4');")
                                              .arg(memeData["name"].toString())
                                              .arg(images[images.size() - 1].toObject()["url"].toString())
                                              .arg(memeData["vk_id"].toString())
                                              .arg(memeData["category"].toString());
            insertMemeQuery.exec(insertMemeQueryString);
        }
    }
    reply->deleteLater();
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
    reply->deleteLater();
}

void UpdatingData::updateUsersPopValues(){
    QSqlQuery namesQuery(database);
    if(namesQuery.exec("SELECT name FROM users")){
        while(namesQuery.next()){
            QString name = namesQuery.value(0).toString();
            QSqlQuery updateQuery(database);
            updateQuery.exec(QString("SELECT users.name, memes.name, pop_values, loyalty, startPopValue, pop_value, "
                                     "user_memes.creativity FROM memes "
                                     "INNER JOIN user_memes ON memes.id = meme_id "
                                     "INNER JOIN users ON users.id = user_id WHERE users.name = '%1';")
                                     .arg(name));
            QSqlRecord rec = updateQuery.record();

            int memePopValuesIndex = rec.indexOf("pop_values");
            int loyaltyIndex = rec.indexOf("loyalty");
            int userPopValueIndex = rec.indexOf("pop_value");
            int startPopValueIndex = rec.indexOf("startPopValue");
            int creativityIndex = rec.indexOf("creativity");

            int popValueChange = 0;
            while(updateQuery.next()){
                QJsonArray popArr = QJsonDocument::fromJson(updateQuery.value(memePopValuesIndex).toByteArray()).array();
                int currentValue = popArr.last().toInt();
                int startPopValue = updateQuery.value(startPopValueIndex).toInt();
                double loyalty = updateQuery.value(loyaltyIndex).toDouble() / 100;
                double creativityRate = updateQuery.value(creativityIndex).toDouble() / 100;
                popValueChange += static_cast<int>((currentValue * (1 + creativityRate) - startPopValue) * loyalty);
            }
            if(updateQuery.size() != 0){
                QSqlQuery query(database);
                updateQuery.last();
                int userPopValue = updateQuery.value(userPopValueIndex).toInt();
                if(userPopValue + popValueChange > 0){
                    query.exec(QString("UPDATE users SET pop_value = pop_value + '%1' WHERE name = '%2';")
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
            int creativityIncrement = 20;
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

void UpdatingData::updateMemeLoyalty()
{
    QSqlQuery memesQuery(database);
    if(memesQuery.exec("SELECT name, endowedCreativity, investedShekels FROM memes;")){
        QSqlRecord rec = memesQuery.record();
        int nameIndex = rec.indexOf("name");
        int creativityIndex = rec.indexOf("endowedCreativity");
        int shekelsIndex = rec.indexOf("investedShekels");
        while(memesQuery.next()){
            QSqlQuery updateQuery(database);
            int contribution = memesQuery.value(creativityIndex).toDouble()
                    - memesQuery.value(shekelsIndex).toInt() / 10;
            int loyalty = 100 + contribution;
            if(loyalty < 0)
                loyalty = 0;
            else if(loyalty > 100)
                loyalty = 100;
            QString updateQueryString = QString("UPDATE memes SET loyalty = '%1' WHERE name = '%2';")
                                                .arg(loyalty)
                                                .arg(memesQuery.value(nameIndex).toString());
            updateQuery.exec(updateQueryString);
        }
    }
    else
        database.open();
}

void UpdatingData::setAuthData()
{
    QFile fil(":/authData.json");
    fil.open(QFile::ReadOnly | QIODevice::Text);
    QString val = fil.readAll();
    fil.close();
    QJsonObject obj = QJsonDocument::fromJson(val.simplified().toUtf8()).object();
    accessToken = obj["access_token"].toString();
}

