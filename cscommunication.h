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

    void checkName(const QString &name);
    void signUp(QJsonObject &jsonObj);
    void getMemeListOfUser(const QJsonObject &jsonObj);
    void getMemeListWithCategory(const QJsonObject &jsonObj);
    void getMemeDataForUser(const QString &memeName, const QString &userName);
    void getMemesCategories();

    QString getName();
    QString getPassword();
    QString getUserToken();

    void connectToDatabase();
    void processingRequest(QJsonObject &jsonObj);

private:
    QSqlDatabase database;
    QTcpSocket *respSock;

signals:
    void nameAvailable(bool val);
public slots:
    void onNameAvailable(bool val);
};

#endif // CSCOMMUNICATION_H
