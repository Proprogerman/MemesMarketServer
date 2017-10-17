#include <QCoreApplication>
#include "myserver.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL,"rus");
    QCoreApplication a(argc, argv);

    MyServer server;

    return a.exec();
}
