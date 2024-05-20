#ifndef EXIF_FILE_H
#define EXIF_FILE_H

#include <QCoreApplication>

#include <libexif/exif-tag.h>
#include <libexif/exif-log.h>
#include <libexif/exif-utils.h>
#include <libexif/exif-data.h>

typedef struct _ExifData ExifData;
struct _ExifData;
typedef struct _ExifMem ExifMem;
struct _ExifMem;

namespace Exif {

namespace Tag {
namespace GPS {
static const ExifTag LATITUDE      = static_cast<ExifTag>(EXIF_TAG_GPS_LATITUDE);
static const ExifTag LONGITUDE     = static_cast<ExifTag>(EXIF_TAG_GPS_LONGITUDE);
static const ExifTag ALTITUDE      = static_cast<ExifTag>(EXIF_TAG_GPS_ALTITUDE);
static const ExifTag LATITUDE_REF  = static_cast<ExifTag>(EXIF_TAG_GPS_LATITUDE_REF);
static const ExifTag LONGITUDE_REF = static_cast<ExifTag>(EXIF_TAG_GPS_LONGITUDE_REF);
static const ExifTag ALTITUDE_REF  = static_cast<ExifTag>(EXIF_TAG_GPS_ALTITUDE_REF);
} // namespace GPS
} // namespace Tag

/// used in ALTITUDE_REF tag
enum class SeaLevel
{
    Above = 0,
    Below = 1
};

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
///
/// Usage:
///
/// Exif::File exif;
/// if (exif.load(filename, false))
/// {
///     for (ExifIfd ifd: { EXIF_IFD_0, EXIF_IFD_1, EXIF_IFD_EXIF, EXIF_IFD_GPS })
///     {
///         qDebug() << "=====================";
///         qDebug() << "IFD" << exif_ifd_get_name(ifd);
///         qDebug() << "=====================";
///         auto values = exif.values(ifd);
///         for (auto tag: values.keys())
///         {
///             qDebug() << exif_tag_get_name_in_ifd(tag, ifd));
///             qDebug() << exif_tag_get_description_in_ifd(tag, ifd);
///             qDebug() << values[tag];
///             qDebug() << "---------------------";
///         }
///     }
/// }
///
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
    File(const QString& fileName, bool createIfEmpty = true) { load(fileName, createIfEmpty); }
    File();
   ~File();

    bool load(const QString& fileName, bool createIfEmpty = true);
    bool save(const QString& fileName);

    Q_DECL_DEPRECATED QVector<ExifRational> uRationalVector(ExifIfd ifd, ExifTag tag) const;
    void setValue(ExifIfd ifd, ExifTag tag, const QVector<ExifRational> urational);

    Q_DECL_DEPRECATED QByteArray ascii(ExifIfd ifd, ExifTag tag) const;
    void setValue(ExifIfd ifd, ExifTag tag, const QByteArray& ascii);
    void setValue(ExifIfd ifd, ExifTag tag, const char* ascii);
    void setValue(ExifIfd ifd, ExifTag tag, const QString& str);
    void setValue(ExifIfd ifd, ExifTag tag, const wchar_t* str);

    void setValue(ExifIfd ifd, ExifTag tag, ExifFormat format, const QByteArray& bytes);

    void remove(ExifIfd ifd, ExifTag tag);

    QMap<ExifTag, QVariant> values(ExifIfd ifd) const;
    QVariant value(ExifIfd ifd, ExifTag tag) const;

    QPixmap thumbnail(int width = 0, int height = 0) const;

    ExifData* data() const;
    ExifContent* content(ExifIfd ifd) const;
    ExifEntry* entry(ExifIfd ifd, ExifTag tag) const;

    Orientation orientation() const;
    int width() const { return mWidth; }
    int height() const { return mHeight; }

    const QString& errorString() const { return mErrorString; }

    friend class FileHelper;
};

} // namespace Exif

#endif // EXIF_FILE_H
