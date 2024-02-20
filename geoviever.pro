QT += core gui widgets location positioning quick quickwidgets

CONFIG += c++17

include(src/3rdparty/libexif/libexif.pri)
include(src/3rdparty/libjpeg/libjpeg.pri)

SOURCES += \
    src/exif/file.cpp \
    src/exif/utils.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/model.cpp \
    src/pixmaplabel.cpp

HEADERS += \
    src/exif/file.h \
    src/exif/utils.h \
    src/mainwindow.h \
    src/model.h \
    src/pixmaplabel.h

FORMS += \
    src/mainwindow.ui

INCLUDEPATH += \
    src \
    src/3rdparty \

