#ifndef UPDATINGDATA_H
#define UPDATINGDATA_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimer>
#include <QTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QMap>
#include <QVector>
#include <QMapIterator>

class UpdatingData : public QObject
{
    Q_OBJECT
public:
    explicit UpdatingData(QObject *parent = nullptr);

    void connectToDatabase();
    //void vkApi(QString postId);
    void vkApi();
    void updateMemesPopValues(QJsonArray arr);
    void getVkResponse(QNetworkReply *reply);
    void updateUsersPopValues();
private:
    QNetworkAccessManager *mngr;
    QTimer *timer;
    QSqlDatabase database;
    QMap<int, QString> memesMap;
    bool isConnected = false;
signals:
    void testSignal();

public slots:
    void onTimerTriggered();
//    void getVkResponse(QNetworkReply *reply);
};

#endif // UPDATINGDATA_H
