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
//    void getUserData(const QString &userName);
    void getUserData(const QJsonObject &jsonObj);
//    void getMemeListOfUser(const QJsonObject &jsonObj);
    void getMemeListWithCategory(const QJsonObject &jsonObj);
    void getMemeDataForUser(const QString &memeName, const QString &userName);
    void getMemeData(const QString &memeName);
    void getMemesCategories();
    void getUsersRating(const QString &userName);
    void forceMeme(const QJsonObject &jsonObj);
    void unforceMeme(const QString &memeName, const QString &userName);
    void increaseLikesQuantity(const QJsonObject &jsonObj);

    QString getName();

    void connectToDatabase();
    void processingRequest(QJsonObject &jsonObj);

    void setUserStatus(bool status);
private:
    QTcpSocket *respSock;
    QSqlDatabase database;

    QString clientName;

    const int memesPopValuesCount = 12;
signals:
    void nameAvailable(bool val);
public slots:
    void onNameAvailable(bool val);
};

#endif // CSCOMMUNICATION_H
