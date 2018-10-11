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
    QFile fil(":/authData.json");
    fil.open(QFile::ReadOnly | QIODevice::Text);
    QString val = fil.readAll();
    fil.close();
    QJsonObject obj = QJsonDocument::fromJson(val.simplified().toUtf8()).object().value("Database").toObject();

    database = QSqlDatabase::addDatabase("QMYSQL");
    database.setHostName("127.0.0.1");
    database.setDatabaseName(obj["name"].toString());
    database.setPort(3306);
    database.setUserName("root");
    database.setPassword(obj["password"].toString());
    if(!database.open())
        qDebug()<<"database is not open!";
    else{
        qDebug()<<"database is open...";
        timer->start(10000);
        creativityTimer->start(60000);
    }
}

void UpdatingData::vkApi(const QVector<QString> &memesVkId){
    QString postsRequestString;
    for(int i = 0; i < memesVkId.size(); i++){
        QString vkId = memesVkId[i];
        if(postsRequestString.isEmpty())
            postsRequestString = "https://api.vk.com/method/wall.getById?posts=";
        postsRequestString.append(vkId);

        if(i < memesVkId.size() - 1 && (i % 99 != 0 || i == 0))
            postsRequestString.append(",");

        if(i == memesVkId.size() - 1 || (i % 99 == 0 && i != 0)){
            postsRequestString.append("&extended=1&fields=members_count&v=5.80&access_token=");
            postsRequestString.append(accessToken);
            QNetworkReply *postsReply = mngr->get(QNetworkRequest(QUrl(postsRequestString)));
            connect(postsReply, &QNetworkReply::finished, [=](){updateMemesPopValues(postsReply);});
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
        int vk_idIndex = rec.indexOf("vk_id");

        QVector<QString> memesVkId;

        while(query.next()){
            memesVkId.append(query.value(vk_idIndex).toString());
        }
        if(!query.next()){
            vkApi(memesVkId);
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
    QVector<QString> namesFromHub;
    for(int i = 0; i < items.size(); i++){
        QJsonObject item = items[i].toObject();
        QJsonObject memeData = QJsonDocument::fromJson(item["text"].toString().simplified().toUtf8()).object();
        QJsonDocument doc;
        doc.setArray(item["sizes"].toArray());
        QSqlQuery insertMemeQuery(database);
        insertMemeQuery.prepare("INSERT INTO memes (name, image, pop_values, vk_id, category) "
                                "VALUES(:name, :image, '[0]', :vk_id, :category) "
                                "ON DUPLICATE KEY UPDATE image = :image, vk_id = :vk_id, category = :category;");
        insertMemeQuery.bindValue(":name", memeData["name"].toString());
        insertMemeQuery.bindValue(":image", QString(doc.toJson(QJsonDocument::Compact)));
        insertMemeQuery.bindValue(":vk_id", memeData["vk_id"].toString());
        insertMemeQuery.bindValue(":category", memeData["category"].toString());
        insertMemeQuery.exec();
        namesFromHub.append(memeData["name"].toString());
    }
    QSqlQuery checkMemesCountQuery(database);
    checkMemesCountQuery.exec("SELECT id FROM memes;");
    if(items.size() < checkMemesCountQuery.size()){
        QString getIdQueryString = "SELECT id FROM memes WHERE name NOT IN (";
        for(int i = 0; i < namesFromHub.size(); i++){
            getIdQueryString.append("'" + namesFromHub[i] + "'" + (i < namesFromHub.size() - 1 ? "," : ");"));
        }
        QSqlQuery getIdQuery(database);
        getIdQuery.exec(getIdQueryString);
        QString deleteMemesQueryString = "DELETE FROM memes WHERE id IN (";
        QString deleteUserMemesQueryString = "DELETE FROM user_memes WHERE meme_id IN (";
        for(int i = 0; i < getIdQuery.size(); i++){
            getIdQuery.next();
            deleteUserMemesQueryString.append("'" + QString::number(getIdQuery.value(0).toInt()) +
                                              "'" + (i < getIdQuery.size() - 1 ? "," : ");"));
            deleteMemesQueryString.append("'" + QString::number(getIdQuery.value(0).toInt()) +
                                          "'" + (i < getIdQuery.size() - 1 ? "," : ");"));
        }
        QSqlQuery deleteUserMemesQuery(database);
        deleteUserMemesQuery.exec(deleteUserMemesQueryString);
        QSqlQuery deleteMemesQuery(database);
        deleteMemesQuery.exec(deleteMemesQueryString);
    }
    reply->deleteLater();
}

void UpdatingData::updateMemesPopValues(QNetworkReply *reply){
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

            unsigned members;
            for(int i = 0; i < groups.size(); i++){
                if(-groups[i].toObject().value("id").toInt() == ownerId)
                    members = groups[i].toObject().value("members_count").toInt();
            }

            unsigned activity = views * likes * comments * reposts;
            int randPart = qrand() % 3 - 1;
            int popValue = qCeil(activity * 1.0 / members) + randPart;
            popValue = popValue < 0 ? 0 : popValue;

            QString postId = QString::number(ownerId) + '_' + QString::number(item["id"].toInt());

            QSqlQuery query(database);
            query.prepare("SELECT pop_values, edited_by_user FROM memes WHERE vk_id = :vkId;");
            query.bindValue(":vkId", postId);
            query.exec();
            QSqlRecord rec = query.record();
            query.first();

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
                updateQuery.prepare("UPDATE memes SET pop_values = :popValues WHERE vk_id = :postId;");
                updateQuery.bindValue(":popValues", QString(doc.toJson(QJsonDocument::Compact)));
                updateQuery.bindValue(":postId", postId);
                updateQuery.exec();
            }
            else{
                QSqlQuery updateQuery;
                updateQuery.prepare("UPDATE memes SET edited_by_user = 0 WHERE vk_id = :postId;");
                updateQuery.bindValue(":postId", postId);
                updateQuery.exec();
            }
        }
        updateUsersPopValues();
        reply->deleteLater();
}

void UpdatingData::updateUsersPopValues(){
    QSqlQuery namesQuery(database);
    if(namesQuery.exec("SELECT name FROM users")){
        while(namesQuery.next()){
            QString name = namesQuery.value(0).toString();
            QSqlQuery updateQuery(database);
            updateQuery.prepare("SELECT users.name, memes.name, pop_values, loyalty, startPopValue, pop_value, "
                                "user_memes.creativity FROM memes "
                                "INNER JOIN user_memes ON memes.id = meme_id "
                                "INNER JOIN users ON users.id = user_id WHERE users.name = :userName;");
            updateQuery.bindValue(":userName", name);
            updateQuery.exec();

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
                    query.prepare("UPDATE users SET pop_value = pop_value + :popValueChange WHERE name = :userName;");
                    query.bindValue(":popValueChange", popValueChange);
                    query.bindValue(":userName", name);
                    query.exec();
                }
                else{
                    query.prepare("UPDATE users SET pop_value = 0 WHERE name = :userName;");
                    query.bindValue(":userName", name);
                    query.exec();
                }
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
                updateQuery.prepare("UPDATE users SET creativity = creativity + :creativityIncrement WHERE name = :userName;");
                updateQuery.bindValue(":creativityIncrement", creativityIncrement);
                updateQuery.bindValue(":userName", name);
                updateQuery.exec();
            }
            else{
                updateQuery.prepare("UPDATE users SET creativity = 100 WHERE name = :userName;");
                updateQuery.bindValue(":userName", name);
                updateQuery.exec();
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
                deleteQuery.prepare("DELETE FROM user_ad WHERE user_id = :userId AND ad_id = :adId;");
                deleteQuery.bindValue(":userId", userAdQuery.value(userIdIndex).toInt());
                deleteQuery.bindValue(":adId", userAdQuery.value(adIdIndex).toInt());
                deleteQuery.exec();
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
            updateQuery.prepare("UPDATE memes SET loyalty = :loyalty WHERE name = :memeName;");
            updateQuery.bindValue(":loyalty", loyalty);
            updateQuery.bindValue(":memeName", memesQuery.value(nameIndex).toString());
            updateQuery.exec();
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
    QJsonObject obj = QJsonDocument::fromJson(val.simplified().toUtf8()).object().value("Vk").toObject();
    accessToken = obj["access_token"].toString();
}

