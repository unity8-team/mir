#-------------------------------------------------
#
# Project created by QtCreator 2013-03-15T18:27:36
#
#-------------------------------------------------

QT       += core
QT       -= gui

TARGET = monjson
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += main.cpp

unix|win32: LIBS += -lqjson
