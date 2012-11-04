#-------------------------------------------------
#
# Project created by QtCreator 2011-03-20T14:35:34
#
#-------------------------------------------------

QT       += core gui

TARGET = DFBVideoMemoryViz
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    rendertarget.cpp \
    rendercontroller.cpp \
    renderallocationitem.cpp

HEADERS  += mainwindow.h \
    rendertarget.h \
    rendercontroller.h \
    renderallocationitem.h

FORMS    += mainwindow.ui

INCLUDEPATH += /home/ilyes/DirectFB-git/src
INCLUDEPATH += /home/ilyes/DirectFB-git/include
INCLUDEPATH += /home/ilyes/DirectFB-git/lib
