#-------------------------------------------------
#
# Project created by QtCreator 2017-06-29T17:46:43
#
#-------------------------------------------------

QT       += core gui concurrent
QT       += datavisualization
QT       += quick
QT       += quickwidgets
QT       += datavisualization

CONFIG += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SVDUI
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS


# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
INCLUDEPATH += ../SVDCore ../SVDCore/third_party ../SVDCore/tools ../SVDCore/core ../SVDCore/outputs visualization

include(../config.pri)


SOURCES += \
        main.cpp \
        mainwindow.cpp \
    modelcontroller.cpp \
    integratetest.cpp \
    visualization/surfacegraph.cpp \
    visualization/topographicseries.cpp \
    visualization/cameracontrol.cpp \
    visualization/landscapevisualization.cpp \
    visualization/colorpalette.cpp \
    aboutdialog.cpp \
    version.cpp \
    visualization/custom3dinputhandler.cpp

HEADERS += \
        mainwindow.h \
    modelcontroller.h \
    integratetest.h \
    visualization/surfacegraph.h \
    visualization/topographicseries.h \
    visualization/cameracontrol.h \
    visualization/landscapevisualization.h \
    visualization/colorpalette.h \
    aboutdialog.h \
    version.h \
    visualization/custom3dinputhandler.h

FORMS += \
        mainwindow.ui \
    visualization/cameracontrol.ui \
    aboutdialog.ui

contains(DEFINES, USE_TENSORFLOW): SOURCES += testdnn.cpp
contains(DEFINES, USE_TENSORFLOW): HEADERS += testdnn.h
contains(DEFINES, USE_TENSORFLOW): FORMS += testdnn.ui


win32 {

*msvc*: {
    CONFIG (release, debug|release): {
        LIBS += -L../Predictor/release -lPredictor
        PRE_TARGETDEPS += ../Predictor/release/Predictor.lib
        LIBS += -L../SVDCore/release -lSVDCore
        PRE_TARGETDEPS += ../SVDCore/release/SVDCore.lib
    } else {
        LIBS += -L../Predictor/debug -lPredictor
        PRE_TARGETDEPS += ../Predictor/debug/Predictor.lib
        LIBS += -L../SVDCore/debug -lSVDCore
        PRE_TARGETDEPS += ../SVDCore/debug/SVDCore.lib
    }
    #LIBS += -L../../tensorflow/lib14cpu -ltensorflow
    LIBS += -L../../../SVDCore/third_party/FreeImage -lFreeImage

    contains(DEFINES, USE_TENSORFLOW): LIBS += -L../../../../tensorflow/lib25 -ltensorflow_cc


} else {
# for example, llvm
    CONFIG (release, debug|release): {
        LIBS += -L../Predictor/release -lPredictor
        PRE_TARGETDEPS += ../Predictor/release/libPredictor.a
        LIBS += -L../SVDCore/release -lSVDCore
        PRE_TARGETDEPS += ../SVDCore/release/libSVDCore.a
    } else {
        LIBS += -L../Predictor/debug -lPredictor
        PRE_TARGETDEPS += ../Predictor/debug/libPredictor.a
        LIBS += -L../SVDCore/debug -lSVDCore
        PRE_TARGETDEPS += ../SVDCore/debug/libSVDCore.a
    }
    contains(DEFINES, USE_TENSORFLOW): LIBS += -L"..\..\..\..\tensorflow\lib14cpu" -ltensorflow
    LIBS += -L"..\..\..\SVDCore\third_party\FreeImage" -lFreeImage
}



#LIBS += -L../../../tensorflow/tensorflow/contrib/cmake/build/RelWithDebInfo -ltensorflow
LIBS += -L../../../../../tensorflow/tensorflow\contrib\cmake\build\protobuf\src\protobuf\RelWithDebInfo -llibprotobuf



#LIBS += -LE:/dev/tensorflow/tensorflow/contrib/cmake/build/RelWithDebInfo -ltensorflow
#LIBS += -LE:\dev\tensorflow\tensorflow\contrib\cmake\build\protobuf\src\protobuf\RelWithDebInfo -llibprotobuf
#LIBS += -L../../tensorflow/lib14 -ltensorflow
#LIBS += -L../../tensorflow/lib14 -llibprotobuf

# for profiling only:
# LIBS += -L"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v8.0/lib/x64" -lcudart
}

linux-g++ {
PRE_TARGETDEPS += ../SVDCore/libSVDCore.a
PRE_TARGETDEPS += ../Predictor/libPredictor.a

# pre-compiled (local)
# PRE_TARGETDEPS += /usr/lib/tensorflow-cpp/libtensorflow_cc.so
PRE_TARGETDEPS += /usr/local/lib/libtensorflow_cc.so
#PRE_TARGETDEPS += /usr/local/lib/libtensorflow_framework.so

LIBS += -L../SVDCore -lSVDCore
LIBS += -L../Predictor -lPredictor

LIBS += -lfreeimage -lprotobuf
# FreeImage on Linux: see https://codeyarns.com/2014/02/11/how-to-install-and-use-freeimage/
# basically sudo apt-get install libfreeimage3 libfreeimage-dev
#LIBS += -L/usr/lib/x86_64-linux-gnu -lfreeimage

#LIBS += -L/usr/lib/tensorflow-cpp/ -libtensorflow_cc.so
contains(DEFINES, USE_TENSORFLOW): INCLUDEPATH += $$PWD/../../../../../../usr/lib/tensorflow-cpp
contains(DEFINES, USE_TENSORFLOW): DEPENDPATH += $$PWD/../../../../../../usr/lib/tensorflow-cpp

}

unix:!macx: LIBS += -L/usr/lib/tensorflow-cpp/ -ltensorflow_cc -ltensorflow_framework

# querying git repo
win32 {
GIT_HASH="\\\"$$quote($$system(git rev-parse --short HEAD))\\\""
GIT_BRANCH="\\\"$$quote($$system(git rev-parse --abbrev-ref HEAD))\\\""
BUILD_TIMESTAMP="\\\"$$quote($$system(date /t))\\\""
DEFINES += GIT_HASH=$$GIT_HASH GIT_BRANCH=$$GIT_BRANCH BUILD_TIMESTAMP=$$BUILD_TIMESTAMP
} else {
GIT_HASH="\\\"$$system(git -C \""$$_PRO_FILE_PWD_"\" rev-parse --short HEAD)\\\""
GIT_BRANCH="\\\"$$system(git -C \""$$_PRO_FILE_PWD_"\" rev-parse --abbrev-ref HEAD)\\\""
BUILD_TIMESTAMP="\\\"$$system(date -u +\""%Y-%m-%dT%H:%M:%SUTC\"")\\\""
DEFINES += GIT_HASH=$$GIT_HASH GIT_BRANCH=$$GIT_BRANCH BUILD_TIMESTAMP=$$BUILD_TIMESTAMP
}


RESOURCES += \
    res/resource.qrc

DISTFILES += \
    res/iland_splash.png

