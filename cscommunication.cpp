#include "cscommunication.h"

#include "updatingdata.h"

#include <QDebug>
#include <QVariant>
#include <QList>
#include <QVector>

#include <QImage>

#include <QByteArray>
#include <QBuffer>

#include <QJsonArray>

#include <iterator>

#include <QDateTime>


CSCommunication::CSCommunication(QTcpSocket *tcpSocket, QObject *parent) : respSock(tcpSocket),
    QObject(parent)
{
    connectToDatabase();
}

CSCommunication::~CSCommunication()
{
    setUserStatus(false);
}

void CSCommunication::connectToDatabase(){
    QFile fil(":/authData.json");
    fil.open(QFile::ReadOnly | QIODevice::Text);
    QString val = fil.readAll();
    fil.close();
    QJsonObject obj = QJsonDocument::fromJson(val.simplified().toUtf8()).object().value("Database").toObject();

    database = QSqlDatabase::addDatabase("QMYSQL", "CscommunicationConnection" + QString::number(respSock->socketDescriptor()));
    database.setHostName("127.0.0.1");
    database.setDatabaseName(obj["name"].toString());
    database.setPort(3306);
    database.setUserName("root");
    database.setPassword(obj["password"].toString());
    if(!database.open())
        qDebug()<<"database is not open!";
    else
        qDebug()<<"database is open...";
}

void CSCommunication::processingRequest(QJsonObject &jsonObj){
    const QString requestType = jsonObj["requestType"].toString();

    setName(jsonObj["user_name"].toString());

    if(requestType == "checkName"){
        checkName(jsonObj["user_name"].toString());
    }
    else if(requestType == "signUp"){
        signUp(jsonObj);
    }
    else if(requestType == "signIn"){
        signIn(jsonObj);
    }
    else if(requestType == "signOut"){
        setUserStatus(false);
    }
    else if(requestType == "getUserData"){
        getUserData(jsonObj);
    }
    else if(requestType == "getMemeData"){
        getMemeData(jsonObj["meme_name"].toString(), jsonObj["user_name"].toString());
    }
    else if(requestType == "getMemeListWithCategory"){
        getMemeListWithCategory(jsonObj);
    }
    else if(requestType == "getAdList"){
        getAdList(jsonObj);
    }
    else if(requestType == "getMemesCategories"){
        getMemesCategories();
    }
    else if(requestType == "getUsersRating"){
        getUsersRating(jsonObj["user_name"].toString());
    }
    else if(requestType == "forceMeme"){
        forceMeme(jsonObj);
    }
    else if(requestType == "increaseLikesQuantity"){
        increaseLikesQuantity(jsonObj);
    }
    else if(requestType == "acceptAd"){
        acceptAd(jsonObj);
    }
    else if(requestType == "unforceMeme"){
        unforceMeme(jsonObj["meme_name"].toString(), jsonObj["user_name"].toString());
    }
    else if(requestType == "rewardUserWithShekels"){
        rewardUserWithShekels(jsonObj["user_name"].toString(), jsonObj["shekels"].toInt());
    }

}

void CSCommunication::setUserStatus(bool status)
{
    if(!getName().isEmpty()){
        QSqlQuery query(database);
        query.prepare("UPDATE users SET online = :online WHERE name = :name;");
        query.bindValue(":online", status);
        query.bindValue(":name", getName());
        query.exec();
        if(!query.exec())
            database.open();
    }
}

bool CSCommunication::writeData(const QByteArray &data)
{
    if(respSock->state() == QAbstractSocket::ConnectedState){
        respSock->write(intToArray(data.size()));
        respSock->write(data);
        return respSock->waitForBytesWritten();
    }
    else
        return false;
}

QByteArray CSCommunication::intToArray(const quint32 &dataSize)
{
    QByteArray temp;
    QDataStream stream(&temp, QIODevice::ReadWrite);
    stream << dataSize;
    return temp;
}

void CSCommunication::checkName(const QString &name){
    QSqlQuery query(database);
    query.prepare("SELECT * FROM users WHERE name = :name;");
    query.bindValue(":name", name);

    if(query.exec()){
        if(query.size() == 0)
            nameAvailableResponse(true, name);
        else
            nameAvailableResponse(false, name);
    }
    else
        database.open();
}

void CSCommunication::nameAvailableResponse(const bool &val, const QString &name){
    QJsonObject jsonObj{
        { "responseType", "checkNameResponse" },
        { "nameAvailable", val },
        { "name", name }
    };

    writeData(QJsonDocument(jsonObj).toBinaryData());
}

void CSCommunication::signUp(QJsonObject &jsonObj){
    QSqlQuery query(database);

    query.prepare("INSERT INTO users (name, passwordHash, creativity, shekels) VALUES(:name, :passwordHash, 100, 100);");
    query.bindValue(":name", jsonObj["user_name"].toString());
    query.bindValue(":passwordHash", jsonObj["passwordHash"].toString());
    QJsonObject responseObj;
    responseObj.insert("responseType", "signUpResponse");
    responseObj.insert("user_name", jsonObj["user_name"].toString());
    if(query.exec()){
        responseObj.insert("created", true);
        setUserStatus(true);
    }
    else{
        responseObj.insert("created", false);
        database.open();
    }
    writeData(QJsonDocument(responseObj).toBinaryData());
}

void CSCommunication::signIn(QJsonObject &jsonObj)
{
    QSqlQuery query(database);
    query.prepare("SELECT passwordHash FROM users WHERE name = :name;");
    query.bindValue(":name", jsonObj["user_name"].toString());
    if(query.exec()){
        QString passwordHash = jsonObj["passwordHash"].toString();
        QSqlRecord rec = query.record();
        query.first();
        QString userPasswordHash = query.value(rec.indexOf("passwordHash")).toString();
        QJsonObject responseObj;
        responseObj.insert("responseType", "signInResponse");
        responseObj.insert("user_name", jsonObj["user_name"].toString());
        if(userPasswordHash == passwordHash){
            responseObj.insert("accessed", true);
            setUserStatus(true);
        }
        else
            responseObj.insert("accessed", false);

        writeData(QJsonDocument(responseObj).toBinaryData());
    }
    else
        database.open();
}

void CSCommunication::getUserData(const QJsonObject &jsonObj)
{    
    QString userName = jsonObj["user_name"].toString();
    QSqlQuery userQuery(database);

    userQuery.prepare("SELECT pop_value, creativity, shekels FROM users WHERE name = :name;");
    userQuery.bindValue(":name", userName);

    QSqlQuery memesQuery(database);
    memesQuery.prepare( "SELECT memes.name, memes.pop_values, startPopValue, loyalty, category, "
                        "user_memes.creativity, memes.image FROM users "
                        "INNER JOIN user_memes ON users.id = user_id "
                        "INNER JOIN memes ON meme_id = memes.id WHERE users.name = :name;");
    memesQuery.bindValue(":name", userName);

    QJsonObject responseObj;
    QJsonObject userImageObj;
    responseObj.insert("responseType", "getUserDataResponse");

    if(userQuery.exec()){
        QSqlRecord userRec = userQuery.record();
        int userPopValueIndex = userRec.indexOf("pop_value");
        int userCreativityIndex = userRec.indexOf("creativity");
        int userShekelsIndex = userRec.indexOf("shekels");
        userQuery.first();
        const int userPopValue = userQuery.value(userPopValueIndex).toInt();

        QSqlQuery userImageQuery(database);
        if(userImageQuery.exec("SELECT rank, members, image FROM rank_image;")){
            QSqlRecord userImageRecord = userImageQuery.record();
            int membersIndex = userImageRecord.indexOf("members");
            int imageIndex = userImageRecord.indexOf("image");
            QVariantList localImages = jsonObj.value("localImages").toArray().toVariantList();
            QString userImageUrl;
            while(userImageQuery.next()){
                if(userImageQuery.value(membersIndex).toInt() <= userPopValue){
                    userImageUrl = userImageQuery.value(imageIndex).toString();
                }
            }
            QString fileName = QUrl(userImageUrl).fileName();
            responseObj.insert("imageName", fileName);
            if(!localImages.contains(fileName)){
                QImage userImage;
                userImage.load(userImageUrl, "PNG");
                QByteArray byteArr;
                QBuffer buff(&byteArr);
                buff.open(QIODevice::ReadWrite);
                userImage.save(&buff, "PNG");
                auto encoded = buff.data().toBase64();
                userImageObj.insert("responseType", "userImageResponse");
                userImageObj.insert("imageName", fileName);
                userImageObj.insert("imageData", QJsonValue(QString::fromLatin1(encoded)));
            }
        }
        responseObj.insert("pop_value", userPopValue);
        responseObj.insert("creativity", userQuery.value(userCreativityIndex).toInt());
        responseObj.insert("shekels", userQuery.value(userShekelsIndex).toInt());

        if(memesQuery.exec()){
            if(memesQuery.size() > 0){
                QSqlRecord rec = memesQuery.record();
                int memeNameIndex = rec.indexOf("name");
                int memePopIndex = rec.indexOf("pop_values");
                int startPopValueIndex = rec.indexOf("startPopValue");
                int loyaltyIndex = rec.indexOf("loyalty");
                int categoryIndex = rec.indexOf("category");
                int creativityIndex = rec.indexOf("creativity");

                int memeImageUrlIndex = rec.indexOf("image");

                QVariantList memeList;
                QVector<QJsonObject> memeImageVector;

                QVariantList localImages = jsonObj.value("localImages").toArray().toVariantList();

                while(memesQuery.next()){
                    QVariantMap memeObj;
                    memeObj.insert("memeName", memesQuery.value(memeNameIndex).toString());
                    memeObj.insert("popValues", QJsonDocument::fromJson(memesQuery.value(memePopIndex).toByteArray()).array());
                    memeObj.insert("startPopValue", memesQuery.value(startPopValueIndex).toInt());
                    memeObj.insert("loyalty", memesQuery.value(loyaltyIndex).toInt());
                    memeObj.insert("category", memesQuery.value(categoryIndex).toString());
                    memeObj.insert("creativity", memesQuery.value(creativityIndex).toInt());

                    QJsonArray sizesArr = QJsonDocument::fromJson(memesQuery.value(memeImageUrlIndex).toByteArray()).array();
                    int s = 0;
                    int sideDiff = abs(sizesArr[0].toObject()["width"].toInt() - jsonObj["screenWidth"].toString().toInt());
                    for(int i = 1; i < sizesArr.size(); i++){
                        if(abs(sizesArr[i].toObject()["width"].toInt() - jsonObj["screenWidth"].toString().toInt()) < sideDiff){
                            sideDiff = abs(sizesArr[i].toObject()["width"].toInt() - jsonObj["screenWidth"].toString().toInt());
                            s = i;
                        }
                    }

                    memeObj.insert("imageName", QUrl(sizesArr[s].toObject()["url"].toString()).fileName());

                    if(!localImages.contains(memeObj["imageName"]))
                        memeObj.insert("imageUrl", sizesArr[s].toObject()["url"].toString());

                    memeList << memeObj;
                    memeObj.clear();
                }
                memesQuery.first();

                responseObj.insert("memeList", QJsonArray::fromVariantList(memeList));
                for(int i = 0; i < memeImageVector.size(); i++){
                    writeData(QJsonDocument(memeImageVector[i]).toBinaryData());
                }
            }
        }
        else
            database.open();

        writeData(QJsonDocument(responseObj).toBinaryData());
        if(!userImageObj.isEmpty()){
            writeData(QJsonDocument(userImageObj).toBinaryData());
        }
    }
    else
        database.open();
}

void CSCommunication::getMemeListWithCategory(const QJsonObject &jsonObj)
{
    QSqlQuery query(database);
    QString category = jsonObj.value("category").toString();
    QString userName = jsonObj.value("user_name").toString();
    query.prepare("SELECT name, image, pop_values, loyalty FROM memes "
                  "WHERE category = :category;");
    query.bindValue(":category", category);

    if(query.exec()){
        QSqlRecord rec = query.record();
        int memeNameIndex = rec.indexOf("name");
        int memePopIndex = rec.indexOf("pop_values");
        int memeImageUrlIndex = rec.indexOf("image");
        int loyaltyIndex = rec.indexOf("loyalty");

        QVariantList memeList;
        QVector<QJsonObject> memeImageVector;

        QVariantList localImages = jsonObj.value("localImages").toArray().toVariantList();

        while(query.next()){
            QVariantMap memeObj;
            memeObj.insert("memeName", query.value(memeNameIndex).toString());
            memeObj.insert("popValues", QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array());
            memeObj.insert("loyalty", query.value(loyaltyIndex).toInt());
            QSqlQuery forceCheckQuery(database);
            forceCheckQuery.prepare("SELECT startPopValue, user_memes.creativity FROM user_memes "
                                    "INNER JOIN users ON users.id = user_id "
                                    "INNER JOIN memes ON memes.id = meme_id "
                                    "WHERE users.name = :userName AND memes.name = :memeName;");
            forceCheckQuery.bindValue(":userName", userName);
            forceCheckQuery.bindValue(":memeName", memeObj["memeName"].toString());
            forceCheckQuery.exec();
            QSqlRecord forceCheckRec = forceCheckQuery.record();
            int startPopValueIndex = forceCheckRec.indexOf("startPopValue");
            int creativityIndex = forceCheckRec.indexOf("creativity");
            forceCheckQuery.first();

            memeObj.insert("forced", forceCheckQuery.size() != 0);
            memeObj.insert("startPopValue", forceCheckQuery.size() != 0 ? forceCheckQuery.value(startPopValueIndex).toInt() : -1);
            memeObj.insert("creativity", forceCheckQuery.size() != 0 ? forceCheckQuery.value(creativityIndex).toInt() : 0);

            QJsonArray sizesArr = QJsonDocument::fromJson(query.value(memeImageUrlIndex).toByteArray()).array();
            int s = 0;
            int sideDiff = abs(sizesArr[0].toObject()["width"].toInt() - jsonObj["screenWidth"].toString().toInt());
            for(int i = 1; i < sizesArr.size(); i++){
                if(abs(sizesArr[i].toObject()["width"].toInt() - jsonObj["screenWidth"].toString().toInt()) < sideDiff){
                    sideDiff = abs(sizesArr[i].toObject()["width"].toInt() - jsonObj["screenWidth"].toString().toInt());
                    s = i;
                }
            }

            memeObj.insert("imageName", QUrl(sizesArr[s].toObject()["url"].toString()).fileName());

            if(!localImages.contains(memeObj["imageName"]))
                memeObj.insert("imageUrl", sizesArr[s].toObject()["url"].toString());
            memeList << memeObj;
            memeObj.clear();
        }

        QJsonObject jsonMemeList;
        jsonMemeList.insert("responseType", "getMemeListWithCategoryResponse");
        jsonMemeList.insert("category", category);
        jsonMemeList.insert("memeList", QJsonArray::fromVariantList(memeList));
        writeData(QJsonDocument(jsonMemeList).toBinaryData());
        for(int i = 0; i < memeImageVector.size(); i++){
            writeData(QJsonDocument(memeImageVector[i]).toBinaryData());
        }
    }
    else
        database.open();
}

void CSCommunication::getAdList(const QJsonObject &jsonObj)
{
    QSqlQuery query(database);
    QString userName = jsonObj.value("user_name").toString();

    query.prepare("SELECT users.id AS user_id, ads.id AS ad_id, ads.name, ads.en_name, ads.reputation, "
                  "offer, discontented, pop_value, ads.image FROM ads "
                  "INNER JOIN reputation_discontented ON reputation_discontented.reputation = ads.reputation "
                  "INNER JOIN users ON users.name = :name;");
    query.bindValue(":name", userName);

    if(query.exec()){
        QSqlRecord rec = query.record();
        int userIdIndex = rec.indexOf("user_id");
        int adIdIndex = rec.indexOf("ad_id");
        int adNameIndex = rec.indexOf("name");
        int adEnNameIndex = rec.indexOf("en_name");
        int reputationIndex = rec.indexOf("reputation");
        int offerIndex = rec.indexOf("offer");
        int discontentedIndex = rec.indexOf("discontented");
        int popValueIndex = rec.indexOf("pop_value");
        int adImageUrlIndex = rec.indexOf("image");

        QVariantList adList;
        QVector<QJsonObject> adImageVector;

        QVariantList adsWithImages = jsonObj.value("localImages").toArray().toVariantList();
        QString lang = jsonObj["lang"].toString();

        while(query.next()){
            QVariantMap adObj;
            adObj.insert("adName", query.value(lang == "ru" ? adNameIndex : adEnNameIndex).toString());
            adObj.insert("reputation", query.value(reputationIndex).toString());
            adObj.insert("discontented", query.value(discontentedIndex).toInt());
            adObj.insert("imageName", QUrl(query.value(adImageUrlIndex).toString()).fileName());

            QSqlQuery timeQuery(database);
            timeQuery.prepare("SELECT unavailableUntil FROM user_ad WHERE user_id = :userId AND ad_id = :adId;");
            timeQuery.bindValue(":userId", QVariant(query.value(userIdIndex).toInt()));
            timeQuery.bindValue(":adId", QVariant(query.value(adIdIndex).toInt()));

            QDateTime unavailableUntil;

            timeQuery.exec();
            timeQuery.first();

            if(timeQuery.size() == 0)
                adObj.insert("secondsToReady", 0);
            else if(timeQuery.value(0).toDateTime() != QDateTime(QDate(1,1,1),QTime(1,1,1))){
                unavailableUntil = timeQuery.value(0).toDateTime();
                adObj.insert("secondsToReady", QDateTime::currentDateTime().secsTo(unavailableUntil));
            }

            int profit = qFloor(query.value(offerIndex).toDouble() / 100 * query.value(popValueIndex).toDouble());
            int maxProfit = 500;
            adObj.insert("profit", profit > maxProfit ? maxProfit : profit);

            if(!adsWithImages.contains(QUrl(query.value(adImageUrlIndex).toString()).fileName())){
                QImage adImage;
                QJsonObject adImageObj;
                adImage.load(query.value(adImageUrlIndex).toString(), "PNG");
                QByteArray byteArr;
                QBuffer buff(&byteArr);
                buff.open(QIODevice::WriteOnly);
                adImage.save(&buff, "PNG");
                auto encoded = buff.data().toBase64();
                adImageObj.insert("responseType", "adImageResponse");
                adImageObj.insert("adName", query.value(adNameIndex).toString());
                adImageObj.insert("imageName", QUrl(query.value(adImageUrlIndex).toString()).fileName());
                adImageObj.insert("imageData", QJsonValue(QString::fromLatin1(encoded)));
                adImageVector.append(adImageObj);
            }
            adList << adObj;
            adObj.clear();
        }
        QJsonObject jsonAdList;
        jsonAdList.insert("responseType", "getAdListResponse");
        jsonAdList.insert("adList", QJsonArray::fromVariantList(adList));
        writeData(QJsonDocument(jsonAdList).toBinaryData());
        for(int i = 0; i < adImageVector.size(); i++){
            writeData(QJsonDocument(adImageVector[i]).toBinaryData());
        }
    }
    else
        database.open();
}

void CSCommunication::getMemeData(const QString &memeName, const QString &userName)
{
    QSqlQuery query(database);
    query.prepare("SELECT image, pop_values, loyalty, category FROM memes "
                  "WHERE name = :name;");
    query.bindValue(":name", memeName);

    if(query.exec()){
        QSqlRecord rec = query.record();
        int memePopIndex = rec.indexOf("pop_values");
        int loyaltyIndex = rec.indexOf("loyalty");
        int memeCategoryIndex = rec.indexOf("category");

        query.first();

        QJsonObject memeDataResponse;
        memeDataResponse.insert("responseType", "getMemeDataResponse");
        memeDataResponse.insert("memeName", memeName);
        memeDataResponse.insert("popValues", QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array());
        memeDataResponse.insert("loyalty", query.value(loyaltyIndex).toInt());
        memeDataResponse.insert("category", query.value(memeCategoryIndex).toString());

        QSqlQuery forceCheckQuery(database);
        forceCheckQuery.prepare("SELECT startPopValue, user_memes.creativity FROM user_memes "
                                "INNER JOIN users ON users.id = user_id "
                                "INNER JOIN memes ON memes.id = meme_id "
                                "WHERE users.name = :userName AND memes.name = :memeName;");
        forceCheckQuery.bindValue(":userName", userName);
        forceCheckQuery.bindValue(":memeName", memeName);
        forceCheckQuery.exec();

        QSqlRecord forceCheckRec = forceCheckQuery.record();
        int startPopValueIndex = forceCheckRec.indexOf("startPopValue");
        int creativityIndex = forceCheckRec.indexOf("creativity");
        forceCheckQuery.first();

        memeDataResponse.insert("forced", forceCheckQuery.size() != 0);
        memeDataResponse.insert("startPopValue", forceCheckQuery.size() != 0 ? forceCheckQuery.value(startPopValueIndex).toInt() : -1);
        memeDataResponse.insert("creativity", forceCheckQuery.size() != 0 ? forceCheckQuery.value(creativityIndex).toInt() : 0);

        writeData(QJsonDocument(memeDataResponse).toBinaryData());
    }
    else
        database.open();
}

void CSCommunication::getMemesCategories()
{
    QSqlQuery query(database);
    if(query.exec("SELECT DISTINCT category FROM memes;")){
        QSqlRecord rec = query.record();
        int categoryIndex = rec.indexOf("category");
        QJsonArray categoryArr;

        while(query.next()){
            categoryArr.append(QJsonValue(query.value(categoryIndex).toString()));
        }
        QJsonObject categoriesResponse;
        categoriesResponse.insert("responseType", "getMemesCategoriesResponse");
        categoriesResponse.insert("categories", categoryArr);
        writeData(QJsonDocument(categoriesResponse).toBinaryData());
    }
    else
        database.open();
}

void CSCommunication::getUsersRating(const QString &userName)
{
    QSqlQuery query(database);
    if(query.exec("SELECT name, pop_value FROM users ORDER BY pop_value DESC;")){
        QSqlRecord rec = query.record();
        int nameIndex = rec.indexOf("name");
        int popValueIndex = rec.indexOf("pop_value");
        QJsonObject responseObj;
        responseObj.insert("responseType", "getUsersRatingResponse");
        QVariantList usersList;
        int rating = 1;
        while(query.next()){
            QVariantMap userMap;
            userMap.insert("rating", rating);
            userMap.insert("user_name", query.value(nameIndex).toString());
            userMap.insert("pop_value", query.value(popValueIndex).toInt());
            if(userName == userMap["user_name"]){
                responseObj.insert("user_rating", rating);
            }
            ++rating;
            usersList << userMap;
        }
        responseObj.insert("usersList", QJsonArray::fromVariantList(usersList));
        writeData(QJsonDocument(responseObj).toBinaryData());
    }
    else
        database.open();
}

void CSCommunication::forceMeme(const QJsonObject &jsonObj)
{
    QSqlQuery query(database);
    QString memeName = jsonObj.value("meme_name").toString();
    QString userName = jsonObj.value("user_name").toString();
    double creativity = jsonObj.value("creativity").toInt();
    query.prepare("SELECT users.id as user_id, memes.id as meme_id FROM users "
                  "INNER JOIN memes ON memes.name = :memeName "
                  "WHERE users.name = :userName;");
    query.bindValue(":memeName", memeName);
    query.bindValue(":userName", userName);

    if(query.exec()){
        QSqlRecord rec = query.record();
        int userIdIndex = rec.indexOf("user_id");
        int memeIdIndex = rec.indexOf("meme_id");
        query.first();
        QSqlQuery forceQuery(database);
        forceQuery.prepare("INSERT INTO user_memes (user_id, meme_id, startPopValue, creativity) "
                           "VALUES (:userId, :memeId, :startPopValue, :creativity);");
        forceQuery.bindValue(":userId", query.value(userIdIndex).toInt());
        forceQuery.bindValue(":memeId", query.value(memeIdIndex).toInt());
        forceQuery.bindValue(":startPopValue", jsonObj.value("startPopValue").toInt());
        forceQuery.bindValue(":creativity", creativity);
        forceQuery.exec();

        if(creativity > 0){
            QSqlQuery updateQuery(database);
            updateQuery.prepare("UPDATE users SET creativity = creativity - :creativity WHERE name = :userName;");
            updateQuery.bindValue(":creativity", creativity);
            updateQuery.bindValue(":userName", userName);
            updateQuery.exec();

            updateQuery.clear();
            updateQuery.prepare("UPDATE memes SET endowedCreativity = endowedCreativity + :creativity WHERE name = :memeName;");
            updateQuery.bindValue(":creativity", creativity);
            updateQuery.bindValue(":memeName", memeName);
            updateQuery.exec();
        }
    }
    else
        database.open();
}

void CSCommunication::acceptAd(const QJsonObject &jsonObj)
{
    QSqlQuery query(database);
    query.prepare("SELECT ads.id AS ad_id, users.id AS user_id, pop_value FROM ads "
                  "INNER JOIN users WHERE ads.name = :adName AND users.name = :userName;");
    query.bindValue(":adName", jsonObj["adName"].toString());
    query.bindValue(":userName", jsonObj["user_name"].toString());

    if(query.exec()){
        query.first();
        QSqlRecord rec = query.record();
        int adIdIndex = rec.indexOf("ad_id");
        int userIdIndex = rec.indexOf("user_id");
        int popValueIndex = rec.indexOf("pop_value");

        int discontented = qFloor(query.value(popValueIndex).toDouble() * (jsonObj["adDiscontented"].toDouble() / 100));
        QSqlQuery acceptQuery(database);
        acceptQuery.prepare("INSERT INTO user_ad (user_id, ad_id, unavailableUntil) VALUES(:userId, :adId, :unavailableUntil);");
        acceptQuery.bindValue(":userId", query.value(userIdIndex).toInt());
        acceptQuery.bindValue(":adId", query.value(adIdIndex).toInt());
        acceptQuery.bindValue(":unavailableUntil", QDateTime::currentDateTime().addSecs(1800).toString("yyyy-MM-dd hh:mm:ss"));

        if(acceptQuery.exec()){
            QSqlQuery shekelsPopQuery(database);
            shekelsPopQuery.prepare("UPDATE users SET shekels = shekels + :profit, pop_value = "
                                    "IF(pop_value >= :discontented, pop_value - '%2', 0) "
                                    "WHERE name = :userName;");
            shekelsPopQuery.bindValue(":profit", jsonObj["adProfit"].toInt());
            shekelsPopQuery.bindValue(":discontented", discontented);
            shekelsPopQuery.bindValue(":userName", jsonObj["user_name"].toString());
            shekelsPopQuery.exec();
        }
    }
}

void CSCommunication::unforceMeme(const QString &memeName, const QString &userName)
{
    QSqlQuery query(database);
    query.prepare( "DELETE u_m FROM user_memes as u_m "
                   "INNER JOIN users as u ON user_id = u.id "
                   "INNER JOIN memes as m ON meme_id = m.id "
                   "WHERE m.name = :memeName and u.name = :userName;");
    query.bindValue(":memeName", memeName);
    query.bindValue(":userName", userName);
    query.exec();
}

void CSCommunication::increaseLikesQuantity(const QJsonObject &jsonObj)
{
    int likePrice = 1;
    int shekels = jsonObj["shekels"].toInt();
    QVariantList popValues = jsonObj["currentPopValues"].toArray().toVariantList();
    int likeIncrement = popValues.last().toInt() + shekels * likePrice;

    if(popValues.size() == memesPopValuesCount){
        for(int i = 0; i < popValues.size() - 1; i++){
            popValues[i] = popValues[i + 1];
        }
        popValues[memesPopValuesCount - 1] = likeIncrement;
    }
    else if(popValues.size() < memesPopValuesCount)
        popValues.append(likeIncrement);

    QSqlQuery likesQuery(database);
    QJsonDocument jsonDoc;
    jsonDoc.setArray(QJsonArray::fromVariantList(popValues));
    likesQuery.prepare("UPDATE memes SET pop_values = :popValues, edited_by_user = 1 WHERE name= :memeName;");
    likesQuery.bindValue(":popValues", QString(jsonDoc.toJson(QJsonDocument::Compact)));
    likesQuery.bindValue(":memeName", jsonObj["meme_name"].toString());

    if(likesQuery.exec()){
        QSqlQuery updateShekelsQuery(database);
        updateShekelsQuery.prepare("UPDATE users SET shekels = shekels - :shekels WHERE name = :userName;");
        updateShekelsQuery.bindValue(":shekels", shekels);
        updateShekelsQuery.bindValue(":userName", jsonObj.value("user_name").toString());
        updateShekelsQuery.exec();

        updateShekelsQuery.clear();
        updateShekelsQuery.prepare("UPDATE memes SET investedShekels = investedShekels + :shekels WHERE name = :memeName;");
        updateShekelsQuery.bindValue(":shekels", shekels);
        updateShekelsQuery.bindValue(":memeName", jsonObj.value("meme_name").toString());
        updateShekelsQuery.exec();
    }
    else
        database.open();
}

void CSCommunication::rewardUserWithShekels(const QString &userName, const int &shekels)
{
    QSqlQuery query(database);
    query.prepare("UPDATE users SET shekels = shekels + :shekels WHERE name = :userName;");
    query.bindValue(":shekels", shekels);
    query.bindValue(":userName", userName);

    if(!query.exec()){
        database.open();
        rewardUserWithShekels(userName, shekels);
    }
}

QString CSCommunication::getName()
{
    return clientName;
}

void CSCommunication::setName(const QString &userName)
{
    clientName = userName;
}


