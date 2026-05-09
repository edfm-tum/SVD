#-------------------------------------------------
#
# Project created by QtCreator 2017-06-26T15:53:37
#
#-------------------------------------------------

QT       -= gui
QT      += concurrent
QT      += core

TARGET = Predictor
TEMPLATE = lib
CONFIG += staticlib
# c++11: for Eigen c++14
CONFIG += c++17
# avoid conflict with eigen library
CONFIG += no_keywords

include(../config.pri)

win32 {
DEFINES += NOMINMAX
}

# SVD modules
INCLUDEPATH += ../SVDCore ../SVDCore/core ../SVDCore/tools ../SVDCore/third_party ../SVDCore/outputs

win32:CONFIG(release, debug|release): DEFINES +=  _ITERATOR_DEBUG_LEVEL=0

win32:CONFIG(debug, debug|release): DEFINES +=  TF_DEBUG_MODE=0

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    batchmanager.cpp \
    batch.cpp \
    inferencedata.cpp \
    dnn.cpp \
    dnnshell.cpp \
    batchdnn.cpp \
    inputtensoritem.cpp \
    fetchdata.cpp \
    predictortest.cpp \
    predtest.cpp

HEADERS += \
    batchmanager.h \
    batch.h \
    tensorhelper.h \
    inferencedata.h \
    dnn.h \
    dnnshell.h \
    batchdnn.h \
    inputtensoritem.h \
    fetchdata.h \
    predictortest.h \
    predtest.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}
