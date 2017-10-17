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

    void setPassword(const QString &password);


    //void setToken(const QString &token);

    void signUp(QJsonObject &jsonObj);

    QString getName();
    QString getPassword();
    QString getUserToken();

    void connectToDatabase();
    void processingRequest(QJsonObject &jsonObj);                    //обработка запроса от клиента

private:
    QSqlDatabase database;
    QTcpSocket *respSock;

    QString user_name;
    QString user_password;
    QString user_token;

signals:
    void nameAvailable(bool val);
public slots:
    void onNameAvailable(bool val);
};

#endif // CSCOMMUNICATION_H
