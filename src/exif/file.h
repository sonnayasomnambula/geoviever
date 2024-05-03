#ifndef EXIF_FILE_H
#define EXIF_FILE_H

#include <QCoreApplication>
#include <QString>

#include <libexif/exif-tag.h>
#include <libexif/exif-log.h>
#include <libexif/exif-utils.h>

typedef struct _ExifData ExifData;
struct _ExifData;
typedef struct _ExifMem ExifMem;
struct _ExifMem;

namespace Exif {

class Orientation
{
    uint16_t mValue;

public:
    enum Type {
        Unknown,
        Normal,
        MirrorHorizontal,
        Rotate180,
        MirrorVertical,
        MirrorHorizontalAndRotate270CW,
        Rotate90CW,
        MirrorHorizontalAndRotate90CW,
        Rotate270CW
    };

    Orientation(uint16_t value = Unknown) : mValue(value) {}
    operator uint16_t() const { return mValue; }

    bool isRotated() const;
};

/// EXIF tags are stored in several groups called IFDs.
/// You can load all tags from the file with load function.
/// Set functions replaces an existing tag in a ifd or creates a new one.
/// You must know the format of the tag in order to get its value.
class File
{
    Q_DECLARE_TR_FUNCTIONS(File)

private:
    QString mFileName;
    ExifData* mExifData = nullptr;
    ExifMem* mAllocator = nullptr;
    ExifLog* mLog = nullptr;

    uint16_t mWidth = 0;
    uint16_t mHeight = 0;

    QString mErrorString;

public:
    File();
   ~File();

    bool load(const QString& fileName, bool createIfEmpty = true);
    bool save(const QString& fileName);

    void setValue(ExifIfd ifd, ExifTag tag, const QVector<ExifRational> urational);
    QVector<ExifRational> uRationalVector(ExifIfd ifd, ExifTag tag) const;

    void setValue(ExifIfd ifd, ExifTag tag, const QByteArray& ascii);
    QByteArray ascii(ExifIfd ifd, ExifTag tag) const;

    // TODO use QVariant
    uint16_t int16u(ExifIfd ifd, ExifTag tag, uint16_t notset = 0) const;
    uint32_t int32u(ExifIfd ifd, ExifTag tag, uint32_t notset = 0) const;

    QPixmap thumbnail(int width = 0, int height = 0) const;

    Orientation orientation() const;
    int width() const { return mWidth; }
    int height() const { return mHeight; }

    const QString& errorString() const { return mErrorString; }

    friend class FileHelper;
};

} // namespace Exif

#endif // EXIF_FILE_H
