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
CONFIG += c++14
# avoid conflict with eigen library
CONFIG += no_keywords

include(../config.pri)

win32_test {
### tensorflow compiled locally
INCLUDEPATH += ../../../tensorflow ../../../tensorflow/tensorflow/contrib/cmake/build  ../../../tensorflow/tensorflow/contrib/cmake/build/external/eigen_archive
INCLUDEPATH += ../../../tensorflow/tensorflow/contrib/cmake/build/external/nsync/public
INCLUDEPATH += ../../../tensorflow/third_party/eigen3 ../../../tensorflow/tensorflow/contrib/cmake/build/protobuf/src/protobuf/src
}

win32 {

DEFINES += NOMINMAX

INCLUDEPATH += ../../tensorflow/lib25/include
INCLUDEPATH += ../../tensorflow/lib25/include/src
}

unix {
# get tensorflow pre-built from here: https://github.com/ika-rwth-aachen/libtensorflow_cc
# INCLUDEPATH += /usr/include/tensorflow-cpp
# default install on system:
INCLUDEPATH += /usr/local/include/tensorflow
# custom version / location
# INCLUDEPATH += /home/werner/dev/tensorflow/libtensorflow_cc2.11/usr/local/include/tensorflow
}
# SVD modules
INCLUDEPATH += ../SVDCore ../SVDCore/core ../SVDCore/tools ../SVDCore/third_party ../SVDCore/outputs

win32_test {

# https://joe-antognini.github.io/machine-learning/windows-tf-project
# DEFINES +=  COMPILER_MSVC
DEFINES += NOMINMAX
#https://gist.github.com/Garoe/a6a82b75ea8277d12829eee81d6d2203
DEFINES +=  WIN32
DEFINES +=  _WINDOWS
DEFINES +=  NDEBUG
DEFINES +=  EIGEN_AVOID_STL_ARRAY
DEFINES +=  _WIN32_WINNT=0x0A00
DEFINES +=  LANG_CXX14
#DEFINES +=  COMPILER_MSVC
DEFINES +=  OS_WIN
DEFINES +=  _MBCS
DEFINES +=  WIN64
DEFINES +=  WIN32_LEAN_AND_MEAN
DEFINES +=  NOGDI
DEFINES +=  TENSORFLOW_USE_EIGEN_THREADPOOL
DEFINES +=  EIGEN_HAS_C99_MATH
DEFINES += GOOGLE_CUDA=1
}



win32:CONFIG(release, debug|release): DEFINES +=  _ITERATOR_DEBUG_LEVEL=0

win32:CONFIG(debug, debug|release): DEFINES +=  TF_DEBUG_MODE=0

#LIBS += -LE:/dev/tensorflow/tensorflow/contrib/cmake/build/RelWithDebInfo -ltensorflow

#LIBS += -L../../tensorflow/lib14 -ltensorflow
#LIBS += -L../../tensorflow/lib14QStringcpu -ltensorflow


# GPU
#LIBS += -L../../tensorflow/lib14 -ltensorflow
# CPU library
# LIBS += -L../../tensorflow/lib14cpu -ltensorflow

# only required for the PredTest example:
# LIBS += -LE:\dev\tensorflow\tensorflow\contrib\cmake\build\protobuf\src\protobuf\RelWithDebInfo -llibprotobuf.lib
# E:\dev\tensorflow\tensorflow\contrib\cmake\build\protobuf\src\protobuf\RelWithDebInfo
# for profiling only:
# LIBS += -L"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v8.0/lib/x64" -lcudart

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
    fetchdata.cpp

HEADERS += \
    batchmanager.h \
    batch.h \
    tensorhelper.h \
    inferencedata.h \
    dnn.h \
    dnnshell.h \
    batchdnn.h \
    inputtensoritem.h \
    fetchdata.h
unix {
    target.path = /usr/lib
    INSTALLS += target
}

# compile only when TF enabled
contains(DEFINES, USE_TENSORFLOW): SOURCES += predictortest.cpp predtest.cpp
contains(DEFINES, USE_TENSORFLOW): HEADERS += predictortest.h predtest.h
