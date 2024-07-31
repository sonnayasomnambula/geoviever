QT += core gui widgets location positioning quick quickwidgets

CONFIG += c++17

include(src/3rdparty/libexif/libexif.pri)
include(src/3rdparty/libjpeg/libjpeg.pri)

SOURCES += \
    src/coordeditdialog.cpp \
    src/exif/file.cpp \
    src/exif/utils.cpp \
    src/exifstorage.cpp \
    src/geocoordinate.cpp \
    src/keywordsdialog.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/model.cpp \
    src/pics.cpp \
    src/pixmaplabel.cpp \
    src/tooltip.cpp

HEADERS += \
    src/coordeditdialog.h \
    src/exif/file.h \
    src/exif/utils.h \
    src/exifstorage.h \
    src/geocoordinate.h \
    src/keywordsdialog.h \
    src/mainwindow.h \
    src/model.h \
    src/pics.h \
    src/pixmaplabel.h \
    src/qtcompat.h \
    src/threadsafe.hpp \
    src/tooltip.h

FORMS += \
    src/mainwindow.ui

INCLUDEPATH += \
    src \
    src/3rdparty \

RESOURCES += \
    rsc/resources.qrc

