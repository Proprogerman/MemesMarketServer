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
//    respSock->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    connect(this, &CSCommunication::nameAvailable, this, &CSCommunication::onNameAvailable);
}

CSCommunication::~CSCommunication()
{
    setUserStatus(false);
}

void CSCommunication::connectToDatabase(){
    database = QSqlDatabase::addDatabase("QMYSQL", "CscommunicationConnection" + QString::number(respSock->socketDescriptor()));
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
    const QString requestType = jsonObj["requestType"].toString();

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
}

void CSCommunication::setUserStatus(bool status)
{
    if(!clientName.isEmpty()){
        QSqlQuery query(database);
        QString queryString = QString("UPDATE users SET online = '%1' WHERE name = '%2';")
                                     .arg(status)
                                     .arg(clientName);
        if(!query.exec(queryString))
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

void CSCommunication::onNameAvailable(bool val){
    QJsonObject jsonObj{
        { "responseType", "checkNameResponse" },
        { "nameAvailable", val }
    };

    writeData(QJsonDocument(jsonObj).toBinaryData());
}

void CSCommunication::checkName(const QString &name){
    QSqlQuery query(database);
    query.prepare("SELECT * FROM users WHERE name = :name;");
    query.bindValue(":name", name);

    if(query.exec()){
        if(query.size() == 0)
            emit nameAvailable(true);
        else
            emit nameAvailable(false);
    }
    else
        database.open();
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
        clientName = jsonObj["user_name"].toString();
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

    QString queryString = QString("SELECT passwordHash FROM users WHERE name = '%1';").arg(jsonObj["user_name"].toString());
    if(query.exec(queryString)){
        QString passwordHash = jsonObj["passwordHash"].toString();
        QSqlRecord rec = query.record();
        query.first();
        QString userPasswordHash = query.value(rec.indexOf("passwordHash")).toString();
        QJsonObject responseObj;
        responseObj.insert("responseType", "signInResponse");
        responseObj.insert("user_name", jsonObj["user_name"].toString());
        if(userPasswordHash == passwordHash){
            responseObj.insert("accessed", true);
            clientName = jsonObj["user_name"].toString();
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
    QString userQueryString = QString( "SELECT pop_value, creativity, shekels FROM users WHERE name = '%1';")
                                       .arg(userName);

    QSqlQuery memesQuery(database);
    QString memesQueryString = QString( "SELECT memes.name, memes.pop_values, startPopValue, loyalty, category, "
                                        "user_memes.creativity, memes.image FROM users "
                                        "INNER JOIN user_memes ON users.id = user_id "
                                        "INNER JOIN memes ON meme_id = memes.id WHERE users.name = '%1';")
                                        .arg(userName);

    QJsonObject responseObj;
    responseObj.insert("responseType", "getUserDataResponse");

    if(userQuery.exec(userQueryString)){
        QSqlRecord userRec = userQuery.record();
        int userPopValueIndex = userRec.indexOf("pop_value");
        int userCreativityIndex = userRec.indexOf("creativity");
        int userShekelsIndex = userRec.indexOf("shekels");
        userQuery.first();
        responseObj.insert("pop_value", userQuery.value(userPopValueIndex).toInt());
        responseObj.insert("creativity", userQuery.value(userCreativityIndex).toInt());
        responseObj.insert("shekels", userQuery.value(userShekelsIndex).toInt());
    }
    else
        database.open();

    if(memesQuery.exec(memesQueryString)){
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

            QVariantList memesToUpdatePopValues = jsonObj.value("updateOnlyPopValues").toArray().toVariantList();

            while(memesQuery.next()){
                QVariantMap memeObj;
                memeObj.insert("memeName", memesQuery.value(memeNameIndex).toString());
                memeObj.insert("popValues", QJsonDocument::fromJson(memesQuery.value(memePopIndex).toByteArray()).array());
                memeObj.insert("startPopValue", memesQuery.value(startPopValueIndex).toInt());
                memeObj.insert("loyalty", memesQuery.value(loyaltyIndex).toInt());
                memeObj.insert("category", memesQuery.value(categoryIndex).toString());
                memeObj.insert("creativity", memesQuery.value(creativityIndex).toInt());
                memeObj.insert("imageName",QUrl(memesQuery.value(memeImageUrlIndex).toString()).fileName());
                if(!memesToUpdatePopValues.contains(QUrl(memesQuery.value(memeImageUrlIndex).toString()).fileName())){
                        QImage memeImage;
                        QJsonObject memeImageObj;
                        memeImage.load(memesQuery.value(memeImageUrlIndex).toString(), "JPG");
                        QByteArray byteArr;
                        QBuffer buff(&byteArr);
                        buff.open(QIODevice::WriteOnly);
                        QImage resizedImage = memeImage.scaled(200, 200, Qt::KeepAspectRatio);
//                        memeImage.save(&buff, "JPG");
                        resizedImage.save(&buff, "JPG");
                        auto encoded = buff.data().toBase64();
                        memeImageObj.insert("responseType", "memeImageResponse");
                        memeImageObj.insert("memeName", memesQuery.value(memeNameIndex).toString());
                        memeImageObj.insert("imageName",QUrl(memesQuery.value(memeImageUrlIndex).toString()).fileName());
                        memeImageObj.insert("imageData", QJsonValue(QString::fromLatin1(encoded)));
                        memeImageVector.append(memeImageObj);
                }
                memeList << memeObj;
                memeObj.clear();
            }
            memesQuery.first();

            responseObj.insert("memeList", QJsonArray::fromVariantList(memeList));
            writeData(QJsonDocument(responseObj).toBinaryData());
            for(int i = 0; i < memeImageVector.size(); i++){
                writeData(QJsonDocument(memeImageVector[i]).toBinaryData());
            }
        }
        else{
            responseObj.insert("memeList", QJsonArray());
            writeData(QJsonDocument(responseObj).toBinaryData());
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
    QString queryString = QString("SELECT name, image, pop_values, loyalty FROM memes "
                                  "WHERE category = '%1';")
                          .arg(category);
    if(query.exec(queryString)){
        QSqlRecord rec = query.record();
        int memeNameIndex = rec.indexOf("name");
        int memePopIndex = rec.indexOf("pop_values");
        int memeImageUrlIndex = rec.indexOf("image");
        int loyaltyIndex = rec.indexOf("loyalty");

        QVariantList memeList;
        QVector<QJsonObject> memeImageVector;

        QVariantList memesToUpdatePopValues = jsonObj.value("updateOnlyPopValues").toArray().toVariantList();

        while(query.next()){
            QVariantMap memeObj;
            memeObj.insert("memeName", query.value(memeNameIndex).toString());
            memeObj.insert("imageName",QUrl(query.value(memeImageUrlIndex).toString()).fileName());
            memeObj.insert("popValues", QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array());
            memeObj.insert("loyalty", query.value(loyaltyIndex).toInt());
            QSqlQuery forceCheckQuery(database);
            QString forceCheckQueryString = QString("SELECT startPopValue, user_memes.creativity FROM user_memes "
                                              "INNER JOIN users ON users.id = user_id "
                                              "INNER JOIN memes ON memes.id = meme_id "
                                              "WHERE users.name = '%1' AND memes.name = '%2';")
                                              .arg(userName)
                                              .arg(memeObj["memeName"].toString());
            forceCheckQuery.exec(forceCheckQueryString);
            QSqlRecord forceCheckRec = forceCheckQuery.record();
            int startPopValueIndex = forceCheckRec.indexOf("startPopValue");
            int creativityIndex = forceCheckRec.indexOf("creativity");
            forceCheckQuery.first();

            memeObj.insert("forced", forceCheckQuery.size() != 0);
            memeObj.insert("startPopValue", forceCheckQuery.size() != 0 ? forceCheckQuery.value(startPopValueIndex).toInt() : -1);
            memeObj.insert("creativity", forceCheckQuery.size() != 0 ? forceCheckQuery.value(creativityIndex).toInt() : 0);

            if(!memesToUpdatePopValues.contains(QUrl(query.value(memeImageUrlIndex).toString()).fileName())){
                    qDebug()<<"does not contain";
                    QImage memeImage;
                    QJsonObject memeImageObj;
                    memeImage.load(query.value(memeImageUrlIndex).toString(), "JPG");
                    QByteArray byteArr;
                    QBuffer buff(&byteArr);
                    buff.open(QIODevice::WriteOnly);
                    QImage resizedImage = memeImage.scaled(200, 200, Qt::KeepAspectRatio);
                    resizedImage.save(&buff, "JPG");
                    auto encoded = buff.data().toBase64();
                    memeImageObj.insert("responseType", "memeImageResponse");
                    memeImageObj.insert("memeName", query.value(memeNameIndex).toString());
                    memeImageObj.insert("imageName",QUrl(query.value(memeImageUrlIndex).toString()).fileName());
                    memeImageObj.insert("imageData", QJsonValue(QString::fromLatin1(encoded)));
                    memeImageVector.append(memeImageObj);
            }
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
    QString queryString = QString("SELECT users.id AS user_id,ads.id AS ad_id,ads.name,ads.reputation,offer,discontented,pop_value,ads.image from ads "
                                  "INNER JOIN reputation_discontented ON reputation_discontented.reputation = ads.reputation "
                                  "INNER JOIN users ON users.name = '%1';")
                                  .arg(userName);
    if(query.exec(queryString)){
        QSqlRecord rec = query.record();
        int userIdIndex = rec.indexOf("user_id");
        int adIdIndex = rec.indexOf("ad_id");
        int adNameIndex = rec.indexOf("name");
        int reputationIndex = rec.indexOf("reputation");
        int offerIndex = rec.indexOf("offer");
        int discontentedIndex = rec.indexOf("discontented");
        int popValueIndex = rec.indexOf("pop_value");
        int adImageUrlIndex = rec.indexOf("image");

        QVariantList adList;
        QVector<QJsonObject> adImageVector;

        QVariantList adsWithImages = jsonObj.value("adsWithImages").toArray().toVariantList();

        while(query.next()){
            QVariantMap adObj;
            adObj.insert("adName", query.value(adNameIndex).toString());
            adObj.insert("reputation", query.value(reputationIndex).toString());
            adObj.insert("discontented", query.value(discontentedIndex).toInt());
            adObj.insert("imageName", QUrl(query.value(adImageUrlIndex).toString()).fileName());

            QSqlQuery timeQuery(database);
            QString timeQueryString = QString("SELECT unavailableUntil FROM user_ad WHERE user_id = '%1' AND ad_id = '%2';")
                                      .arg(query.value(userIdIndex).toInt())
                                      .arg(query.value(adIdIndex).toInt());

            QDateTime unavailableUntil;

            timeQuery.exec(timeQueryString);
            timeQuery.first();

            if(timeQuery.size() == 0)
                adObj.insert("secondsToReady", 0);
            else if(timeQuery.value(0).toDateTime() != QDateTime(QDate(1,1,1),QTime(1,1,1))){
                unavailableUntil = timeQuery.value(0).toDateTime();
                adObj.insert("secondsToReady", QDateTime::currentDateTime().secsTo(unavailableUntil));
            }

            int profit = qFloor(query.value(offerIndex).toDouble() / 100 * query.value(popValueIndex).toDouble());
            adObj.insert("profit", profit);

            if(!adsWithImages.contains(QUrl(query.value(adImageUrlIndex).toString()).fileName())){
                    QImage adImage;
                    QJsonObject adImageObj;
                    adImage.load(query.value(adImageUrlIndex).toString(), "PNG");
                    QByteArray byteArr;
                    QBuffer buff(&byteArr);
                    buff.open(QIODevice::WriteOnly);
                    QImage resizedImage = adImage.scaled(200, 200, Qt::KeepAspectRatio);
                    resizedImage.save(&buff, "PNG");
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
        respSock->waitForBytesWritten(3000);
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
    QString queryString = QString("SELECT image, pop_values, loyalty, category FROM memes "
                                  "WHERE name = '%1';")
                                  .arg(memeName);
    if(query.exec(queryString)){
        QSqlRecord rec = query.record();
        int memePopIndex = rec.indexOf("pop_values");
        int memeImageUrlIndex = rec.indexOf("image");
        int loyaltyIndex = rec.indexOf("loyalty");
        int memeCategoryIndex = rec.indexOf("category");

        query.first();

        QJsonObject memeDataResponse;
        memeDataResponse.insert("responseType", "getMemeDataResponse");
        memeDataResponse.insert("memeName", memeName);
        memeDataResponse.insert("popValues", QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array());
        memeDataResponse.insert("loyalty", query.value(loyaltyIndex).toInt());
        memeDataResponse.insert("category", query.value(memeCategoryIndex).toString());
        memeDataResponse.insert("imageName", QUrl(query.value(memeImageUrlIndex).toString()).fileName());

        QSqlQuery forceCheckQuery(database);
        QString forceCheckQueryString = QString("SELECT startPopValue, user_memes.creativity FROM user_memes "
                                          "INNER JOIN users ON users.id = user_id "
                                          "INNER JOIN memes ON memes.id = meme_id "
                                          "WHERE users.name = '%1' AND memes.name = '%2';")
                                          .arg(userName)
                                          .arg(memeName);
        forceCheckQuery.exec(forceCheckQueryString);
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
    QString queryString = QString(  "SELECT users.id as user_id, memes.id as meme_id, weight FROM users "
                                    "INNER JOIN memes ON memes.name = '%1' "
                                    "INNER JOIN category_weight ON category_weight.category = memes.category "
                                    "WHERE users.name = '%2';")
                                    .arg(memeName)
                                    .arg(userName);
    if(query.exec(queryString)){
        QSqlRecord rec = query.record();
        int userIdIndex = rec.indexOf("user_id");
        int memeIdIndex = rec.indexOf("meme_id");
//        int weightIndex = rec.indexOf("weight");
        query.first();
        QSqlQuery forceQuery(database);
        forceQuery.exec(QString("INSERT INTO user_memes (user_id, meme_id, startPopValue, creativity) "
                                "VALUES ('%1', '%2', '%3', '%4');")
                                .arg(query.value(userIdIndex).toInt())
                                .arg(query.value(memeIdIndex).toInt())
                                .arg(jsonObj.value("startPopValue").toInt())
                                .arg(creativity));
        if(creativity > 0){
            QSqlQuery updateQuery(database);
            updateQuery.exec(QString("UPDATE users SET creativity = creativity - '%1' WHERE name = '%2';")
                                    .arg(creativity)
                                    .arg(userName));
            updateQuery.clear();
            updateQuery.exec(QString("UPDATE memes SET endowedCreativity = endowedCreativity + '%1' WHERE name = '%2';")
                                    .arg(creativity)
                                    .arg(memeName));
        }
    }
    else
        database.open();
}

void CSCommunication::acceptAd(const QJsonObject &jsonObj)
{
    QSqlQuery query(database);
    QString queryString = QString("SELECT ads.id AS ad_id, users.id AS user_id, pop_value FROM ads "
                                  "INNER JOIN users WHERE ads.name = '%1' AND users.name = '%2';")
                          .arg(jsonObj["adName"].toString())
                          .arg(jsonObj["user_name"].toString());
    if(query.exec(queryString)){
        query.first();
        QSqlRecord rec = query.record();
        int adIdIndex = rec.indexOf("ad_id");
        int userIdIndex = rec.indexOf("user_id");
        int popValueIndex = rec.indexOf("pop_value");

        int discontented = qFloor(query.value(popValueIndex).toDouble() * (jsonObj["adDiscontented"].toDouble() / 100));
        QSqlQuery acceptQuery(database);
        QString acceptQueryString = QString("INSERT INTO user_ad (user_id, ad_id, unavailableUntil) VALUES('%1','%2','%3');")
                                    .arg(query.value(userIdIndex).toInt())
                                    .arg(query.value(adIdIndex).toInt())
                                    .arg(QDateTime::currentDateTime().addSecs(1800).toString("yyyy-MM-dd hh:mm:ss"));

        if(acceptQuery.exec(acceptQueryString)){
            QSqlQuery shekelsPopQuery(database);
            QString shekelsPopQueryString = QString("UPDATE users SET shekels = shekels + '%1', pop_value = "
                                                    "IF(pop_value >= '%2', pop_value - '%2', 0) "
                                                    "WHERE name = '%3';")
                                            .arg(jsonObj["adProfit"].toInt())
                                            .arg(discontented)
                                            .arg(jsonObj["user_name"].toString());
            shekelsPopQuery.exec(shekelsPopQueryString);
        }
    }
}

void CSCommunication::unforceMeme(const QString &memeName, const QString &userName)
{
    QSqlQuery query(database);
    QString queryString = QString(  "DELETE u_m FROM user_memes as u_m "
                                    "INNER JOIN users as u ON user_id = u.id "
                                    "INNER JOIN memes as m ON meme_id = m.id "
                                    "WHERE m.name = '%1' and u.name = '%2';")
                                    .arg(memeName)
                                    .arg(userName);
    query.exec(queryString);
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
    QString queryString = QString("UPDATE memes SET pop_values = '%1', edited_by_user = 1 WHERE name='%2';")
            .arg(QString(jsonDoc.toJson(QJsonDocument::Compact)))
            .arg(jsonObj["meme_name"].toString());
    if(likesQuery.exec(queryString)){
        QSqlQuery updateShekelsQuery(database);
        updateShekelsQuery.exec(QString("UPDATE users SET shekels = shekels - '%1' WHERE name = '%2';")
                                       .arg(shekels)
                                       .arg(jsonObj.value("user_name").toString()));
        updateShekelsQuery.clear();
        updateShekelsQuery.exec(QString("UPDATE memes SET investedShekels = investedShekels + '%1' WHERE name = '%2';")
                                        .arg(shekels)
                                        .arg(jsonObj["meme_name"].toString()));
    }
    else
        database.open();
}

void CSCommunication::rewardUserWithShekels(const QString &userName, const int &shekels)
{
    QSqlQuery query(database);
    QString queryString = QString("UPDATE users SET shekels = shekels + '%1' WHERE name = '%2';")
            .arg(shekels)
            .arg(userName);

    if(!query.exec(queryString)){
        database.open();
        rewardUserWithShekels(userName, shekels);
    }
}


