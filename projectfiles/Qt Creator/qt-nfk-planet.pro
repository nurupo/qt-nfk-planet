#-------------------------------------------------
#
# Project created by QtCreator 2014-05-02T17:59:52
#
#-------------------------------------------------

QT       += core network

QT       -= gui

TARGET = qt-nfk-planet
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT


SOURCES += \
    ../../src/main.cpp \
    ../../src/client.cpp \
    ../../src/server.cpp \
    ../../src/planet.cpp \
    ../../src/settings.cpp

HEADERS += \
    ../../src/client.h \
    ../../src/server.h \
    ../../src/planet.h \
    ../../src/settings.h

RESOURCES += \
    ../../resources/resources.qrc
