#include "cscommunication.h"

#include <QDebug>
#include <QVariant>
#include <QList>
#include <QVector>

#include <QImage>

#include <QByteArray>
#include <QBuffer>

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
    const QString requestType = jsonObj["requestType"].toString();
    qDebug()<<requestType;

    if(requestType == "checkName"){
        checkName(jsonObj["user_name"].toString());
    }
    else if(requestType == "signUp"){
        signUp(jsonObj);
    }
    else if(requestType == "getMemeListOfUser"){
        getMemeListOfUser(jsonObj);
    }
    else if(requestType == "getMemeDataForUser"){
        getMemeDataForUser(jsonObj["meme_name"].toString(), jsonObj["user_name"].toString());
    }
    else if(requestType == "getMemeListWithCategory"){
        getMemeListWithCategory(jsonObj);
    }
    else if(requestType == "getMemesCategories"){
        getMemesCategories();
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

void CSCommunication::getMemeListOfUser(const QJsonObject &jsonObj)
{
    qDebug()<<"USER_NAME" <<jsonObj["user_name"].toString();
    QSqlQuery query(database);
    query.exec( QString("SELECT memes.name, memes.pop_values, startPopValue, memes.image FROM users "
                        "INNER JOIN user_memes ON users.id = user_id "
                        "INNER JOIN memes ON meme_id = memes.id WHERE users.name = '%1';")
                        .arg(jsonObj["user_name"].toString()));

    QSqlRecord rec = query.record();
    qDebug()<<"query record:"<<rec;
    int memeNameIndex = rec.indexOf("name");
    int memePopIndex = rec.indexOf("pop_values");
    int memeImageUrlIndex = rec.indexOf("image");
    int startPopValueIndex = rec.indexOf("startPopValue");

    qDebug() << "QUERY SIZE= " << query.size();

    QVariantList memeList;

    QVariantList memesToUpdatePopValues = jsonObj.value("updateOnlyPopValues").toArray().toVariantList();
    qDebug()<<"OOOOOOOOOOOOOOOOOOOO ::::::::::: " << memesToUpdatePopValues;

    while(query.next()){
        QVariantMap memeObj;
        memeObj.insert("memeName", query.value(memeNameIndex).toString());
        memeObj.insert("popValues", QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array());
        memeObj.insert("startPopValue", query.value(startPopValueIndex).toInt());
        qDebug()<<"POP VALUES FROM GETMEMELISTWITHCATEGORY ::::::::" << QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array();
        if(!memesToUpdatePopValues.contains(query.value(memeNameIndex).toString())){
                qDebug()<<"NE SODERJIT!!!!!!!!!!!!!!!!!!!!!!!!!!";
                QImage memeImage;
                memeImage.load(query.value(memeImageUrlIndex).toString(), "JPG");
                QByteArray byteArr;
                QBuffer buff(&byteArr);
                buff.open(QIODevice::WriteOnly);
                memeImage.save(&buff, "JPG");
                auto encoded = buff.data().toBase64();
                memeObj.insert("imageName",QUrl(query.value(memeImageUrlIndex).toString()).fileName());
                memeObj.insert("imageData", QJsonValue(QString::fromLatin1(encoded)));
        }
        memeList << memeObj;
        memeObj.clear();
    }
    QJsonObject jsonMemeList;
    jsonMemeList.insert("responseType", "getMemeListOfUserResponse");
    jsonMemeList.insert("memeList", QJsonArray::fromVariantList(memeList));
    respSock->write(QJsonDocument(jsonMemeList).toBinaryData());
    respSock->waitForBytesWritten(3000);
}

void CSCommunication::getMemeListWithCategory(const QJsonObject &jsonObj)
{
    QSqlQuery query(database);
    QString category = jsonObj.value("category").toString();
    query.exec( QString("SELECT name, image, pop_values FROM memes WHERE category = '%1';")
                        .arg(category));

    QSqlRecord rec = query.record();
    qDebug()<<"query record:"<<rec;
    qDebug()<<"category: " << category;
    int memeNameIndex = rec.indexOf("name");
    int memePopIndex = rec.indexOf("pop_values");
    int memeImageUrlIndex = rec.indexOf("image");

    QVariantList memeList;

    QVariantList memesToUpdatePopValues = jsonObj.value("updateOnlyPopValues").toArray().toVariantList();
    qDebug()<<"OOOOOOOOOOOOOOOOOOOO ::::::::::: " << memesToUpdatePopValues;

    while(query.next()){
        QVariantMap memeObj;
        memeObj.insert("memeName", query.value(memeNameIndex).toString());
        memeObj.insert("popValues", QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array());
        qDebug()<<"POP VALUES FROM GETMEMELISTOFUSER ::::::::" << QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array();
        if(!memesToUpdatePopValues.contains(query.value(memeNameIndex).toString())){
                qDebug()<<"NE SODERJIT!!!!!!!!!!!!!!!!!!!!!!!!!!";
                QImage memeImage;
                memeImage.load(query.value(memeImageUrlIndex).toString(), "JPG");
                QByteArray byteArr;
                QBuffer buff(&byteArr);
                buff.open(QIODevice::WriteOnly);
                memeImage.save(&buff, "JPG");
                auto encoded = buff.data().toBase64();
                memeObj.insert("imageName",QUrl(query.value(memeImageUrlIndex).toString()).fileName());
                memeObj.insert("imageData", QJsonValue(QString::fromLatin1(encoded)));
        }
        memeList << memeObj;
        memeObj.clear();
    }
    QJsonObject jsonMemeList;
    jsonMemeList.insert("responseType", "getMemeListWithCategoryResponse");
    jsonMemeList.insert("category", category);
    jsonMemeList.insert("memeList", QJsonArray::fromVariantList(memeList));
    respSock->write(QJsonDocument(jsonMemeList).toBinaryData());
    respSock->waitForBytesWritten(3000);
}

void CSCommunication::getMemeDataForUser(const QString &memeName, const QString &userName)
{
    qDebug()<<"MEME_NAME" << memeName;
    QSqlQuery query(database);
    query.exec( QString("SELECT pop_values, startPopValue FROM memes "
                        "INNER JOIN user_memes ON memes.id = meme_id "
                        "INNER JOIN users ON users.id = user_id WHERE memes.name = '%1' AND users.name = '%2';")
                        .arg(memeName)
                        .arg(userName));

    QSqlRecord rec = query.record();
    qDebug()<<"query record:"<<rec;
    int memePopIndex = rec.indexOf("pop_values");
    int startPopValueIndex = rec.indexOf("startPopValue");

    query.next();

    QJsonObject memeDataResponse;
    memeDataResponse.insert("responseType", "getMemeDataForUserResponse");
    memeDataResponse.insert("memeName", memeName);
    memeDataResponse.insert("popValues", QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array());
    memeDataResponse.insert("startPopValue", query.value(startPopValueIndex).toInt());
    qDebug()<<"POP VALUES FOR MEME " << memeName << " ::::::: " << QJsonDocument::fromJson(query.value(memePopIndex).toByteArray()).array();
    respSock->write(QJsonDocument(memeDataResponse).toBinaryData());
    respSock->waitForBytesWritten(3000);
}

void CSCommunication::getMemesCategories()
{
    QSqlQuery query(database);
    query.exec("SELECT DISTINCT category FROM memes;");
    QSqlRecord rec = query.record();
    qDebug()<<"query record:"<<rec;
    int categoryIndex = rec.indexOf("category");
    QJsonArray categoryArr;

    while(query.next()){
        categoryArr.append(QJsonValue(query.value(categoryIndex).toString()));
    }
    QJsonObject categoriesResponse;
    categoriesResponse.insert("responseType", "getMemesCategoriesResponse");
    categoriesResponse.insert("categories", categoryArr);
    respSock->write(QJsonDocument(categoriesResponse).toBinaryData());
    respSock->waitForBytesWritten(3000);
}


