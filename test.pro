QT -= gui
QT += location positioning

CONFIG += c++11 console
CONFIG -= app_bundle

GOOGLETEST_DIR = src/test/google
include(google_dependency.pri)

INCLUDEPATH += \
    src \
    src/3rdparty/sigvdr.de \
    src/3rdparty/libexif

SOURCES += \
    src/3rdparty/sigvdr.de/qexifimageheader.cpp \
    src/exif/utils.cpp \
    src/test/tmpjpegfile.cpp \
    src/test/tst_qexifimageheader.cpp

HEADERS += \
    src/3rdparty/sigvdr.de/qexifimageheader.h \
    src/exif/utils.h \
    src/test/tmpjpegfile.h

RESOURCES += \
    rsc/test.qrc
