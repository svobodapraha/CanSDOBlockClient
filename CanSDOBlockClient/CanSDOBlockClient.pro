#-------------------------------------------------
#
# Project created by QtCreator 2016-12-03T20:35:00
#
#-------------------------------------------------

QT       += core gui
QT       += serialbus

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = CanSDOBlockClient
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    CanSDOBlockClient.c

HEADERS  += mainwindow.h \
    CanSDOBlockClient.h

FORMS    += mainwindow.ui

QMAKE_CFLAGS += -std=c11
