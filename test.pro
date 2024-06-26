QT -= gui
QT += location positioning

CONFIG += c++11 console
CONFIG -= app_bundle

include(src/3rdparty/libexif/libexif.pri)
include(src/3rdparty/libjpeg/libjpeg.pri)

GOOGLETEST_DIR = src/test/google
include(google_dependency.pri)

INCLUDEPATH += \
    src \
    src/3rdparty/libexif

SOURCES += \
    src/exif/file.cpp \
    src/exif/utils.cpp \
    src/exifstorage.cpp \
    src/pics.cpp \
    src/test/tmpjpegfile.cpp \
    src/test/tst_exiffile.cpp

HEADERS += \
    src/exif/file.h \
    src/exif/utils.h \
    src/exifstorage.h \
    src/pics.h \
    src/test/tmpjpegfile.h

RESOURCES += \
    rsc/test.qrc
