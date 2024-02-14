QT += core gui widgets location positioning quick quickwidgets

CONFIG += c++17

include(src/3rdparty/libexif/libexif.pri)
include(src/3rdparty/libjpeg/libjpeg.pri)

SOURCES += \
    src/exif.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/model.cpp \
    src/pixmaplabel.cpp

HEADERS += \
    src/exif.h \
    src/mainwindow.h \
    src/model.h \
    src/pixmaplabel.h

FORMS += \
    src/mainwindow.ui

INCLUDEPATH += \
    src \
    src/3rdparty \

