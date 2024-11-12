QT -= gui
QT += core concurrent

CONFIG += c++11 console
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS


# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += ../SVDCore ../SVDCore/third_party ../SVDCore/tools ../SVDCore/core ../SVDCore/outputs


SOURCES += \
    consoleshell.cpp \
        main.cpp \
    ../SVDUI/modelcontroller.cpp \
    ../SVDUI/version.cpp


HEADERS += \
    ../SVDUI/modelcontroller.h \
    ../SVDUI/version.h \
    consoleshell.h


win32:CONFIG (release, debug|release): LIBS += -L../Predictor/release -lPredictor
else:win32:CONFIG (debug, debug|release): LIBS += -L../Predictor/debug -lPredictor

# https://forum.qt.io/topic/22298/solved-change-of-library-but-creator-does-not-build-completely
win32:CONFIG (release, debug|release): PRE_TARGETDEPS += ../Predictor/release/Predictor.lib
else:win32:CONFIG (debug, debug|release): PRE_TARGETDEPS += ../Predictor/debug/Predictor.lib


win32:CONFIG (release, debug|release): LIBS += -L../SVDCore/release -lSVDCore
else:win32:CONFIG (debug, debug|release): LIBS += -L../SVDCore/debug -lSVDCore

win32:CONFIG (release, debug|release): PRE_TARGETDEPS += ../SVDCore/release/SVDCore.lib
else:win32:CONFIG (debug, debug|release): PRE_TARGETDEPS += ../SVDCore/debug/SVDCore.lib

win32 {
LIBS += -L../../../tensorflow/tensorflow/contrib/cmake/build/RelWithDebInfo -ltensorflow
LIBS += -L../../../tensorflow\tensorflow\contrib\cmake\build\protobuf\src\protobuf\RelWithDebInfo -llibprotobuf
LIBS += -L../../SVDModel/SVDCore/third_party/FreeImage -lFreeImage

# for profiling only:
# LIBS += -L"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v8.0/lib/x64" -lcudart
}
linux-g++ {
PRE_TARGETDEPS += ../SVDCore/libSVDCore.a
PRE_TARGETDEPS += ../Predictor/libPredictor.a

# pre-compiled (local)
# PRE_TARGETDEPS += /usr/lib/tensorflow-cpp/libtensorflow_cc.so
# PRE_TARGETDEPS += /usr/local/lib/libtensorflow_cc.so
# PRE_TARGETDEPS += /home/werner/dev/tensorflow/libtensorflow_cc2.11/usr/local/lib/libtensorflow_cc.so
#PRE_TARGETDEPS += /usr/local/lib/libtensorflow_framework.so

LIBS += -L../SVDCore -lSVDCore
LIBS += -L../Predictor -lPredictor
#LIBS += -L/usr/lib/tensorflow-cpp/ -libtensorflow_cc.so
LIBS += -L/usr/lib/x86_64-linux-gnu -lfreeimage
LIBS += -Lusr/local/lib -ltensorflow_cc -lprotobuf -ltensorflow_framework

}

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


# standard linux location
# unix:!macx: LIBS += -L/usr/lib/tensorflow-cpp/ -ltensorflow_cc

# INCLUDEPATH += $$PWD/../../../../../../usr/lib/tensorflow-cpp
# DEPENDPATH += $$PWD/../../../../../../usr/lib/tensorflow-cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
