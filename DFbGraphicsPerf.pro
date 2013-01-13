#-------------------------------------------------
#
# Project created by QtCreator 2011-03-20T14:35:34
#
#-------------------------------------------------

QT       += core gui

TARGET = DFbGraphicsPerf
TEMPLATE = app


SOURCES += main.cpp\
    rendertarget.cpp \
    renderallocationitem.cpp \
    tracecontrollerdialog.cpp \
    mainwindow.cpp \
    controllerscene.cpp \
    allocationrendercontroller.cpp

HEADERS  += \
    rendertarget.h \
    renderallocationitem.h \
    tracecontrollerdialog.h \
    mainwindow2.h \
    mainwindow.h \
    controllerscene.h \
    allocationrendercontroller.h

FORMS    += \
    tracecontrollerdialog.ui \
    mainwindow.ui

INCLUDEPATH += /home/ilyes/DirectFB-git/src
INCLUDEPATH += /home/ilyes/DirectFB-git/include
INCLUDEPATH += /home/ilyes/DirectFB-git/lib
