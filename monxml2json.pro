#-------------------------------------------------
#
# Project created by QtCreator 2013-03-15T16:22:24
#
#-------------------------------------------------

QT       += core
QT       += xml
QT       -= gui

TARGET = monxml2json
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp

unix|win32: LIBS += -lqjson
