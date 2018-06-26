#ifndef CSCOMMUNICATION_H
#define CSCOMMUNICATION_H

#include <QObject>

#include <QtSql>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>

#include <QTcpSocket>

class CSCommunication : public QObject
{
    Q_OBJECT
public:
    explicit CSCommunication(QTcpSocket *tcpSocket, QObject *parent = nullptr);
    ~CSCommunication();

    void checkName(const QString &name);
    void signUp(QJsonObject &jsonObj);
    void signIn(QJsonObject &jsonObj);
    void getUserData(const QJsonObject &jsonObj);
    void getMemeListWithCategory(const QJsonObject &jsonObj);
    void getAdList(const QJsonObject &jsonObj);
    void getMemeData(const QString &memeName, const QString &userName);
    void getMemesCategories();
    void getUsersRating(const QString &userName);
    void forceMeme(const QJsonObject &jsonObj);
    void acceptAd(const QJsonObject &jsonObj);
    void unforceMeme(const QString &memeName, const QString &userName);
    void increaseLikesQuantity(const QJsonObject &jsonObj);
    void rewardUserWithShekels(const QString &userName, const int &shekels);

    QString getName();

    void connectToDatabase();
    void processingRequest(QJsonObject &jsonObj);

    void setUserStatus(bool status);

    bool writeData(const QByteArray &data);
    QByteArray intToArray(const quint32 &dataSize);
private:
    QTcpSocket *respSock;
    QSqlDatabase database;

    QString clientName;

    const int memesPopValuesCount = 12;
signals:
    void nameAvailable(bool val);
    void adAccepted();
public slots:
    void onNameAvailable(bool val);
};

#endif // CSCOMMUNICATION_H
