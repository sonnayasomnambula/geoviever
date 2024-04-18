# https://github.com/libexif/libexif
# TODO build a library

HEADERS += \
    $$PWD/libexif/apple/exif-mnote-data-apple.h \
    $$PWD/libexif/apple/mnote-apple-entry.h \
    $$PWD/libexif/apple/mnote-apple-tag.h \
    $$PWD/libexif/canon/exif-mnote-data-canon.h \
    $$PWD/libexif/canon/mnote-canon-entry.h \
    $$PWD/libexif/canon/mnote-canon-tag.h \
    $$PWD/libexif/exif-byte-order.h \
    $$PWD/libexif/exif-content.h \
    $$PWD/libexif/exif-data-type.h \
    $$PWD/libexif/exif-data.h \
    $$PWD/libexif/exif-entry.h \
    $$PWD/libexif/exif-format.h \
    $$PWD/libexif/exif-gps-ifd.h \
    $$PWD/libexif/exif-ifd.h \
    $$PWD/libexif/exif-loader.h \
    $$PWD/libexif/exif-log.h \
    $$PWD/libexif/exif-mem.h \
    $$PWD/libexif/exif-mnote-data-priv.h \
    $$PWD/libexif/exif-mnote-data.h \
    $$PWD/libexif/exif-system.h \
    $$PWD/libexif/exif-tag.h \
    $$PWD/libexif/exif-utils.h \
    $$PWD/libexif/exif.h \
    $$PWD/libexif/fuji/exif-mnote-data-fuji.h \
    $$PWD/libexif/fuji/mnote-fuji-entry.h \
    $$PWD/libexif/fuji/mnote-fuji-tag.h \
    $$PWD/libexif/i18n.h \
    $$PWD/libexif/olympus/exif-mnote-data-olympus.h \
    $$PWD/libexif/olympus/mnote-olympus-entry.h \
    $$PWD/libexif/olympus/mnote-olympus-tag.h \
    $$PWD/libexif/pentax/exif-mnote-data-pentax.h \
    $$PWD/libexif/pentax/mnote-pentax-entry.h \
    $$PWD/libexif/pentax/mnote-pentax-tag.h

SOURCES += \
    $$PWD/libexif/apple/exif-mnote-data-apple.c \
    $$PWD/libexif/apple/mnote-apple-entry.c \
    $$PWD/libexif/apple/mnote-apple-tag.c \
    $$PWD/libexif/canon/exif-mnote-data-canon.c \
    $$PWD/libexif/canon/mnote-canon-entry.c \
    $$PWD/libexif/canon/mnote-canon-tag.c \
    $$PWD/libexif/exif-byte-order.c \
    $$PWD/libexif/exif-content.c \
    $$PWD/libexif/exif-data.c \
    $$PWD/libexif/exif-entry.c \
    $$PWD/libexif/exif-format.c \
    $$PWD/libexif/exif-gps-ifd.c \
    $$PWD/libexif/exif-ifd.c \
    $$PWD/libexif/exif-loader.c \
    $$PWD/libexif/exif-log.c \
    $$PWD/libexif/exif-mem.c \
    $$PWD/libexif/exif-mnote-data.c \
    $$PWD/libexif/exif-tag.c \
    $$PWD/libexif/exif-utils.c \
    $$PWD/libexif/fuji/exif-mnote-data-fuji.c \
    $$PWD/libexif/fuji/mnote-fuji-entry.c \
    $$PWD/libexif/fuji/mnote-fuji-tag.c \
    $$PWD/libexif/olympus/exif-mnote-data-olympus.c \
    $$PWD/libexif/olympus/mnote-olympus-entry.c \
    $$PWD/libexif/olympus/mnote-olympus-tag.c \
    $$PWD/libexif/pentax/exif-mnote-data-pentax.c \
    $$PWD/libexif/pentax/mnote-pentax-entry.c \
    $$PWD/libexif/pentax/mnote-pentax-tag.c

INCLUDEPATH += \
    $$PWD


STDINT_FILE = $${LITERAL_HASH}include<stdint.h>
write_file($$OUT_PWD/libexif/_stdint.h, STDINT_FILE)
write_file($$OUT_PWD/config.h)
DEFINES += GETTEXT_PACKAGE # TODO
