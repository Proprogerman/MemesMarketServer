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
#include <QFile>

class UpdatingData : public QObject
{
    Q_OBJECT
public:
    explicit UpdatingData(QObject *parent = nullptr);
    ~UpdatingData();

    void connectToDatabase();
    void vkApi(const QVector<QString> &memesVkId);
    void updateUsersPopValues();
    void updateUsersCreativity();
    void updateUserAdTime();
    void updateMemeLoyalty();
    void setAuthData();
private:
    QNetworkAccessManager *mngr;
    QTimer *timer;
    QTimer *creativityTimer;
    QSqlDatabase database;
    bool isConnected = false;
    QString accessToken;

    const int memesPopValuesCount = 12;
signals:
    void testSignal();

public slots:
    void onTimerTriggered();
    void checkMemesFromHub(QNetworkReply *reply);
    void updateMemesPopValues(QNetworkReply *reply);
};

#endif // UPDATINGDATA_H
