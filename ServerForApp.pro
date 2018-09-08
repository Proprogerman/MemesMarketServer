QT += core network sql gui

CONFIG += c++11

TARGET = ServerForApp
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    myserver.cpp \
    cscommunication.cpp \
    updatingdata.cpp \
    serverrunnable.cpp

DEFINES += QT_DEPRECATED_WARNINGS

HEADERS += \
    myserver.h \
    cscommunication.h \
    updatingdata.h \
    serverrunnable.h

RESOURCES += \
    serverres.qrc
