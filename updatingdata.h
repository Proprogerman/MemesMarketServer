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
    ~UpdatingData();

    void connectToDatabase();
    void vkApi();
    void updateUsersPopValues();
    void updateUsersCreativity();
    void updateUserAdTime();
    void updateMemeLoyalty();
private:
    QNetworkAccessManager *mngr;
    QTimer *timer;
    QTimer *creativityTimer;
    QSqlDatabase database;
    QMap<int, QString> memesMap;
    QMap<int, int> postsCount;
    bool isConnected = false;

    const int memesPopValuesCount = 12;
signals:
    void testSignal();

public slots:
    void onTimerTriggered();
    void updateMemesPopValues(QNetworkReply *reply);
    void setPostsCount(QNetworkReply *reply);
};

#endif // UPDATINGDATA_H
