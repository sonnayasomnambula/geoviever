#include <QBuffer>
#include <QByteArray>
#include <QDebug>
#include <QImageReader>
#include <QPixmap>
#include <QVector>

#include <cstdio>

#include <libexif/exif-content.h>
#include <libexif/exif-data.h>
#include <libexif/exif-loader.h>
#include <libjpeg/jpeg-data.h>

#include "exif/file.h"

#include "pics.h"

namespace Exif
{

bool Orientation::isRotated() const
{
    switch (mValue) {
    case MirrorHorizontalAndRotate270CW:
    case Rotate90CW:
    case MirrorHorizontalAndRotate90CW:
    case Rotate270CW:
        return true;
    }

    return false;
}

class FileHelper // TODO use Qt private class
{
public:
    static const QByteArray AsciiMarker;
    static const QByteArray UnicodeMarker;
    static const QByteArray JisMarker;

    static void log(ExifLog* /*log*/, ExifLogCode code, const char* domain, const char* format, va_list args, void* self)
    {
        constexpr size_t size = 512;
        char buffer[size];
        vsnprintf(buffer, size, format, args);

        QString& message = reinterpret_cast<File*>(self)->mErrorString;
        message = QString("[%1] %2").arg(domain).arg(buffer);

        (code == EXIF_LOG_CODE_DEBUG ? qDebug() : qWarning()).noquote() << message;
    }

    static void warning(ExifEntry* e, const char* message)
    {
        qWarning("Tag 0x%04X '%s': %s",
                 e->tag,
                 e->parent && e->parent->parent ?
                                exif_tag_get_name_in_ifd(e->tag, (ExifIfd)std::distance(e->parent->parent->ifd,
                                                                                        std::find(e->parent->parent->ifd, e->parent->parent->ifd + EXIF_IFD_COUNT, e->parent))) :
                                exif_tag_get_name(e->tag),
                 message);
    }

    template <class T>
    static T trimTrailingNull(T&& value)
    {
        while (value.endsWith('\0'))
            value.resize(value.size() - 1);
        return std::move(value);
    }

    template <typename T>
    static T integer(const unsigned char* buf, ExifByteOrder order) {
        static_assert(std::is_integral<T>::value, "T must be integral type");
        T value = 0;
        if (buf)
        {
            switch (order) {
            case EXIF_BYTE_ORDER_MOTOROLA:
                for (size_t i = 0; i < sizeof(T); ++i) {
                    value <<= 8;
                    value |= static_cast<T>(buf[i]);
                }
                break;
            case EXIF_BYTE_ORDER_INTEL:
                for (size_t i = sizeof(T); i > 0; --i) {
                    value <<= 8;
                    value |= static_cast<T>(buf[i-1]);
                }
                break;
            }
        }
        return value;
    }

    template <typename T>
    static double rational(const unsigned char* buf, ExifByteOrder order) {
        T numerator   = buf ? integer<T>(buf, order) : 0;
        T denominator = buf ? integer<T>(buf + 4, order) : 0;
        return denominator ? (double)numerator / (double)denominator : (double)numerator;
    }

    template <typename T>
    static QVariant decodeInteger(ExifEntry* e, ExifByteOrder o)
    {
        if (e->components == 1)
            return integer<T>(e->data, o);

        QVariantList list;
        list.reserve(e->components);
        for (size_t i = 0; i < e->components; i++) {
            list.append(integer<T> (e->data + exif_format_get_size (e->format) * i, o));
        }
        return list;
    }

    template <typename T>
    static QVariant decodeRational(ExifEntry* e, ExifByteOrder o)
    {
        if (e->components == 1)
            return rational<T>(e->data, o);

        QVariantList list;
        list.reserve(e->components);
        for (size_t i = 0; i < e->components; i++) {
            list.append(rational<T>(e->data + exif_format_get_size (e->format) * i, o));
        }
        return list;
    }

    static QVariant decodeAscii(ExifEntry* e)
    {
        // It should be ASCII here, but Windows Explorer doesn't care and writes UTF-8
        return trimTrailingNull(QString::fromUtf8((const char*)e->data, e->size));
    }

    static QVariant decodeUtf16LE(ExifEntry* e)
    {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        return trimTrailingNull(QString::fromUtf16(reinterpret_cast<uint16_t*>(e->data), e->size / 2));
#else
        static_assert(false, "not implemented", Q_FUNC_INFO);
#endif
    }

    static QVariant decodeRaw(ExifEntry* e)
    {
        Q_ASSERT(AsciiMarker.size() == 8);
        Q_ASSERT(UnicodeMarker.size() == 8);
        Q_ASSERT(JisMarker.size() == 8);

        if (e->components == 1 && e->size == 1)
            return e->data[0];

        if ((e->size >= (unsigned)AsciiMarker.size()) && !memcmp (e->data, AsciiMarker.data(), AsciiMarker.size()))
            return trimTrailingNull(QString::fromLatin1((const char *) e->data + 8, e->size - 8));

        if ((e->size >= (unsigned)UnicodeMarker.size()) && !memcmp (e->data, UnicodeMarker.data(), UnicodeMarker.size()))
            return trimTrailingNull(QString::fromUtf8((const char *) e->data + 8, e->size - 8));

        if ((e->size >= (unsigned)JisMarker.size()) && !memcmp (e->data, JisMarker.data(), JisMarker.size()))
        {
            warning(e, "JIS strings are not supported");
            return {};
        }

        return QByteArray((const char*)e->data, e->size);
    }

    static QVariant decodeDefault(ExifEntry* e)
    {
        if (!e->size) return {};

        if (!e || !e->parent || !e->parent->parent) return {};
        auto o = exif_data_get_byte_order(e->parent->parent);

        switch (e->format) {
        case EXIF_FORMAT_UNDEFINED:
        case EXIF_FORMAT_BYTE:
        case EXIF_FORMAT_SBYTE:
            return decodeRaw(e);
        case EXIF_FORMAT_SHORT:
            return decodeInteger<ExifShort>(e, o);
        case EXIF_FORMAT_SSHORT:
            return decodeInteger<ExifSShort>(e, o);
        case EXIF_FORMAT_LONG:
            return decodeInteger<ExifLong>(e, o);
        case EXIF_FORMAT_SLONG:
            return decodeInteger<ExifSLong>(e, o);
        case EXIF_FORMAT_ASCII:
            return decodeAscii(e);
        case EXIF_FORMAT_RATIONAL:
            return decodeRational<ExifLong>(e, o);
        case EXIF_FORMAT_SRATIONAL:
            return decodeRational<ExifSLong>(e, o);
        case EXIF_FORMAT_DOUBLE:
        case EXIF_FORMAT_FLOAT:
            break; // TODO libexif does not support EXIF_FORMAT_DOUBLE and EXIF_FORMAT_FLOAT, but...
        }

        warning(e, "unable to decode");
        return {};
    }

    static QVariant decode(ExifEntry* e)
    {
        if (e->size != e->components * exif_format_get_size(e->format))
        {
            warning(e, "invalid size");
            return {};
        }

        if (e->size && !e->data)
        {
            warning(e, "no data found");
            return {};
        }

        switch (e->tag)
        {
        case EXIF_TAG_USER_COMMENT:
            if (e->format == EXIF_FORMAT_UNDEFINED) // EXIF_FORMAT_ASCII can be decoded by default
                return decodeRaw(e);
            break;
        case EXIF_TAG_EXIF_VERSION:
            if (e->components == 4)
                return decodeRaw(e);
            break;
        case EXIF_TAG_FLASH_PIX_VERSION:
        case EXIF_TAG_COMPONENTS_CONFIGURATION:
            if (e->format == EXIF_FORMAT_UNDEFINED && e->components == 4)
                return decodeRaw(e);
            break;
        case EXIF_TAG_FILE_SOURCE:
        case EXIF_TAG_SCENE_TYPE:
            if (e->format == EXIF_FORMAT_UNDEFINED && e->components == 1)
                return decodeRaw(e);
            break;
        case EXIF_TAG_XP_TITLE:
        case EXIF_TAG_XP_COMMENT:
        case EXIF_TAG_XP_AUTHOR:
        case EXIF_TAG_XP_KEYWORDS:
        case EXIF_TAG_XP_SUBJECT:
            return decodeUtf16LE(e);
        case EXIF_TAG_INTEROPERABILITY_VERSION:
            // NB! EXIF_TAG_INTEROPERABILITY_VERSION == EXIF_TAG_GPS_LATITUDE
            if (e->format == EXIF_FORMAT_UNDEFINED)
                return decodeRaw(e);
            break;
        default:
            break; // make GCC happy
        }

        return decodeDefault(e);
    }

    static ExifEntry* allocate(ExifIfd ifd, ExifTag tag, size_t size, File* file)
    {
        if (auto data = file->mExifData) {
            if (auto content = data->ifd[ifd]) {
                if (auto entry = exif_content_get_entry(content, tag)) {
                    if (entry->size == size) {
                        return entry;
                    }

                    entry->data = reinterpret_cast<unsigned char*>(exif_mem_realloc(file->mAllocator, entry->data, size));
                    entry->size = size;
                    return entry;
                } else {
                    entry = exif_entry_new_mem(file->mAllocator);
                    entry->data = reinterpret_cast<unsigned char*>(exif_mem_alloc(file->mAllocator, size));
                    entry->size = size;
                    entry->tag = tag;
                    exif_content_add_entry(content, entry);
                    exif_entry_unref(entry);
                    return entry;
                }
            }
        }

        return nullptr;
    }

    static void erase(ExifIfd ifd, ExifTag tag, File* file)
    {
        if (auto data = file->mExifData)
            if (auto content = data->ifd[ifd])
                if (auto entry = exif_content_get_entry(content, tag))
                        exif_content_remove_entry(content, entry);
    }

    static void setUtf16LE(ExifIfd ifd, ExifTag tag, ExifFormat format, const QString& str, File* file)
    {
        if (auto entry = allocate(ifd, tag, str.size() * 2, file))
        {
            QVector<uint16_t> encoded;
            encoded.reserve(str.size());

            for (const QChar& ch : qAsConst(str))
                encoded.append(ch.unicode());

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            memcpy(entry->data, encoded.data(), entry->size);
#else
            static_assert(false, "not implemented", Q_FUNC_INFO);
#endif

            entry->components = entry->size;
            entry->format = format;
        }
    }

    static void setRaw(ExifIfd ifd, ExifTag tag, ExifFormat format, const QByteArray& bytes, File* file)
    {
        if (auto entry = allocate(ifd, tag, bytes.size(), file))
        {
            memcpy(entry->data, bytes.data(), entry->size);
            entry->components = entry->size;
            entry->format = format;
        }
    }
};


const QByteArray FileHelper::AsciiMarker = QByteArrayLiteral("ASCII\0\0\0");
const QByteArray FileHelper::UnicodeMarker = QByteArrayLiteral("UNICODE\0");
const QByteArray FileHelper::JisMarker = QByteArrayLiteral("JIS\0\0\0\0\0");


File::File()
{
    mAllocator = exif_mem_new_default();
    if ((mLog = exif_log_new_mem(mAllocator)))
        exif_log_set_func(mLog, &FileHelper::log, this);
}

File::~File()
{
    exif_log_unref(mLog);
    exif_mem_unref(mAllocator);
}

static inline uint16_t little_endian_to_big_endian(uint16_t value) {
    return (value & 0xFF00) >> 8 | (value & 0x00FF) << 8;
}

static inline bool read_big_endian(uint16_t* value, FILE* file) {
    if (fread(value, sizeof(*value), 1, file) != 1)
        return false;
    *value = little_endian_to_big_endian(*value);
    return true;
}

/// \brief load all EXIF tags from \a fileName;
/// creates an empty storage if there are no tags in the file
bool File::load(const QString& fileName, bool createIfEmpty)
{
    mFileName = fileName;

    std::wstring ws = mFileName.toStdWString();
    const wchar_t* path = ws.c_str();

    // some copy-paste from exif-data.c modified to support wchar_t

    {
        // exif_data_new_from_file

        ExifData* edata;
        ExifLoader *loader;

        loader = exif_loader_new ();

        {
            // exif_loader_write_file

            FILE* f;
            int size;
            unsigned char data[1024];

            if (!loader || !path)
                return false;

            f = _wfopen (path, L"rb");
            if (!f) {
                mErrorString = tr("[%1] The file '%2' could not be opened.")
                                   .arg("ExifLoader")
                                   .arg(path);
                qWarning().noquote() << mErrorString;
                return false;
            }
            while (1) {
                size = fread (data, 1, sizeof (data), f);
                if (size <= 0)
                    break;
                if (!exif_loader_write (loader, data, size))
                    break;
            }

            // image width and height can be found in some JPEG start-of-frame markers
            // TODO update libjpeg and don't stradat hernya (one can use QImageReader::size() as well)

            uint16_t marker_size = 0;

            fseek(f, 2, SEEK_SET); // skip 0xFFD8 starting signature
            while (1) {
                while (fgetc(f) != 0xFF && !feof(f)) /* go ahead */;
                int marker = fgetc(f);
                if (!JPEG_IS_MARKER(marker))
                    continue;

                if (!read_big_endian(&marker_size, f))
                    break;

                if (marker == JPEG_MARKER_SOF0 ||   // Baseline
                    marker == JPEG_MARKER_SOF1 ||   // Extended sequential, Huffman
                    marker == JPEG_MARKER_SOF2 ||   // Progressive, Huffman
                    marker == JPEG_MARKER_SOF3 ||   // Lossless, Huffman
                    marker == JPEG_MARKER_SOF9 ||   // Extended sequential, arithmetic
                    marker == JPEG_MARKER_SOF10 ||  // Progressive, arithmetic
                    marker == JPEG_MARKER_SOF11) {  // Lossless, arithmetic
                    fseek(f, 1, SEEK_CUR);
                    if (!read_big_endian(&mHeight, f)) break;
                    if (!read_big_endian(&mWidth, f)) break;
                    if (mWidth && mHeight) break; // got it, ok
                    if (marker_size > 5) {
                        fseek(f, marker_size - 5, SEEK_CUR); // skip the rest of the marker
                    }
                } else if (marker_size > 2) {
                    fseek(f, marker_size - 2, SEEK_CUR); // skip this marker
                } else {
                    break; // something unexpected
                }
            }

            fclose (f);
        }

        edata = exif_loader_get_data (loader);
        exif_loader_unref (loader);

        mExifData = edata;

        if (orientation().isRotated())
            std::swap(mWidth, mHeight);
    }

    if (!mExifData)
    {
        if (!createIfEmpty)
            return false;

        mExifData = exif_data_new();
        exif_data_fix(mExifData);
        exif_data_set_option(mExifData, EXIF_DATA_OPTION_FOLLOW_SPECIFICATION);
        exif_data_set_data_type(mExifData, EXIF_DATA_TYPE_COMPRESSED);
        exif_data_set_byte_order(mExifData, EXIF_BYTE_ORDER_INTEL);
    }

    return mExifData;
}

bool File::save(const QString& fileName)
{
    JPEGData *data = jpeg_data_new ();

    if (mLog)
        jpeg_data_log(data, mLog);

    std::wstring ws = fileName.toStdWString();
    const wchar_t* path = ws.data();

    // some copy-paste from jpeg-data.c modified to support wchar_t

    {
        // jpeg_data_load_file

        FILE *f;
        unsigned char *d;
        unsigned int size;

        if (!data) return false;
        if (!path) return false;

        f = _wfopen (path, L"rb");
        if (!f) {
            mErrorString = tr("[%1] Path '%2' invalid.")
                               .arg("jpeg-data")
                               .arg(path);
            qWarning().noquote() << mErrorString;
            return false;
        }

        /* For now, we read the data into memory. Patches welcome... */
        fseek (f, 0, SEEK_END);
        size = ftell (f);
        fseek (f, 0, SEEK_SET);
        d = (unsigned char*) malloc (size);
        if (!d) {
            EXIF_LOG_NO_MEMORY (mLog, "jpeg-data", size);
            fclose (f);
            return false;
        }
        if (fread (d, 1, size, f) != size) {
            free (d);
            fclose (f);
            mErrorString = tr("[%1] Could not read '%2'.")
                               .arg("jpeg-data")
                               .arg(path);
            qWarning().noquote() << mErrorString;
            return false;
        }
        fclose (f);

        jpeg_data_load_data (data, d, size);
        free (d);
    }

    jpeg_data_set_exif_data(data, mExifData);

    {
        // jpeg_data_save_file

        FILE *f;
        unsigned char *d = NULL;
        unsigned int size = 0, written;

        jpeg_data_save_data (data, &d, &size);
        if (!d)
            return false;

        _wremove (path);
        f = _wfopen (path, L"wb");
        if (!f) {
            free (d);
            return false;
        }
        written = fwrite (d, 1, size, f);
        fclose (f);
        free (d);
        if (written == size)  {
            return true;
        }
        _wremove(path);
        return false;
    }
}

void File::setValue(ExifIfd ifd, ExifTag tag, const QVector<ExifRational> urational)
{
    if (!mExifData) return;

    ExifEntry* entry = exif_content_get_entry(mExifData->ifd[ifd], tag);
    void* memory;

    const size_t components = urational.size();
    const size_t size = components * exif_format_get_size(EXIF_FORMAT_RATIONAL);

    if (entry)
    {
        if (entry->components == components)
        {
            memory = entry->data;
        }
        else
        {
            memory = exif_mem_realloc(mAllocator, entry->data, size);
        }
    }
    else
    {
        entry = exif_entry_new_mem(mAllocator);
        exif_content_add_entry(mExifData->ifd[ifd], entry);
        exif_entry_initialize(entry, tag);
        memory = exif_mem_alloc(mAllocator, size);
    }

    entry->format = EXIF_FORMAT_RATIONAL;
    entry->components = components;
    entry->size = size;
    entry->data = static_cast<unsigned char*>(memory);

    for (int i = 0; i < urational.size(); ++i)
    {
        exif_set_rational(entry->data + 8 * i, exif_data_get_byte_order(mExifData), urational[i]);
    }
}

QVector<ExifRational> File::uRationalVector(ExifIfd ifd, ExifTag tag) const
{
    QVector<ExifRational> value;
    ExifEntry* entry = mExifData ? exif_content_get_entry(mExifData->ifd[ifd], tag) : nullptr;
    if (!entry) return value;

    value.reserve(entry->components);
    for (size_t i = 0; i < entry->components; ++i)
        value.append(exif_get_rational(entry->data + i * 8, exif_data_get_byte_order(mExifData)));

    return value;
}


void File::setValue(ExifIfd ifd, ExifTag tag, const QByteArray& ascii)
{
    if (!mExifData) return;

    if (ascii.isEmpty())
    {
        FileHelper::erase(ifd, tag, this);
        return;
    }

    switch (tag)
    {
    case EXIF_TAG_XP_TITLE:
    case EXIF_TAG_XP_COMMENT:
    case EXIF_TAG_XP_AUTHOR:
    case EXIF_TAG_XP_KEYWORDS:
    case EXIF_TAG_XP_SUBJECT:
        return FileHelper::setUtf16LE(ifd, tag, EXIF_FORMAT_BYTE, QString::fromUtf8(ascii), this);
    case EXIF_TAG_EXIF_VERSION:
    case EXIF_TAG_USER_COMMENT: // in QByteArray version, user comment is copied as is
        return FileHelper::setRaw(ifd, tag, EXIF_FORMAT_UNDEFINED, ascii, this);
    default:
        return FileHelper::setRaw(ifd, tag, EXIF_FORMAT_ASCII, ascii, this);
    }
}

void File::setValue(ExifIfd ifd, ExifTag tag, const char *ascii)
{
    setValue(ifd, tag, QByteArray::fromRawData(ascii, strlen(ascii)));
}

QByteArray File::ascii(ExifIfd ifd, ExifTag tag) const
{
    ExifEntry* entry = mExifData ? exif_content_get_entry(mExifData->ifd[ifd], tag) : nullptr;
    if (!entry) return {};

    QByteArray d(reinterpret_cast<char*>(entry->data), entry->size);
    if (d.endsWith('\0'))
        d.resize(d.size() - 1);
    return d;
}

QMap<ExifTag, QVariant> File::values(ExifIfd ifd) const
{
    QMap<ExifTag, QVariant> values;
    if (auto content = this->content(ifd))
    {
        // cannot capture anything
        exif_content_foreach_entry(content, [](ExifEntry* entry, void* user_data){
                QVariant v = FileHelper::decode(entry);
                if (!v.isNull())
                    if (auto valuesPtr = reinterpret_cast<QMap<ExifTag, QVariant>*>(user_data))
                        (*(valuesPtr))[entry->tag] = v;
            }, &values);
    }
    return values;
}

QVariant File::value(ExifIfd ifd, ExifTag tag) const
{
    if (auto entry = exif_content_get_entry(content(ifd), tag))
        return FileHelper::decode(entry);
    return {};
}

void File::setValue(ExifIfd ifd, ExifTag tag, const QString& str)
{
    if (!mExifData) return;

    if (str.isEmpty())
    {
        FileHelper::erase(ifd, tag, this);
        return;
    }

    switch (tag)
    {
    case EXIF_TAG_XP_TITLE:
    case EXIF_TAG_XP_COMMENT:
    case EXIF_TAG_XP_AUTHOR:
    case EXIF_TAG_XP_KEYWORDS:
    case EXIF_TAG_XP_SUBJECT:
        return FileHelper::setUtf16LE(ifd, tag, EXIF_FORMAT_BYTE, str, this);
    case EXIF_TAG_EXIF_VERSION:
        return FileHelper::setRaw(ifd, tag, EXIF_FORMAT_UNDEFINED, str.toUtf8(), this);
    case EXIF_TAG_USER_COMMENT:
        return FileHelper::setRaw(ifd, tag, EXIF_FORMAT_UNDEFINED, FileHelper::UnicodeMarker + str.toUtf8(), this);
    default:
        return FileHelper::setRaw(ifd, tag, EXIF_FORMAT_ASCII, str.toUtf8(), this);
    }
}

void File::setValue(ExifIfd ifd, ExifTag tag, const wchar_t* str)
{
    return setValue(ifd, tag, QString::fromWCharArray(str));
}

void File::setValue(ExifIfd ifd, ExifTag tag, ExifFormat format, const QByteArray& bytes)
{
    FileHelper::setRaw(ifd, tag, format, bytes, this);
}

void File::remove(ExifIfd ifd, ExifTag tag)
{
    FileHelper::erase(ifd, tag, this);
}

QPixmap File::thumbnail(int width, int height, Thumbnail type) const
{
    if ((type & Thumbnail::Embedded) && mExifData && mExifData->data && mExifData->size)
    {
        QByteArray data = QByteArray::fromRawData(reinterpret_cast<const char*>(mExifData->data), mExifData->size); // not copied
        QBuffer buffer(&data);
        QImageReader reader(&buffer);
        Orientation orientation = value(EXIF_IFD_1, EXIF_TAG_ORIENTATION).toInt();

        // fix non-rotated EXIF thumbnail
        QSize size = reader.size();
        if (orientation == Orientation::Unknown && ((mWidth > mHeight) != (size.width() > size.height())))
        {
            // We don't know whether the picture should be rotated 90CW or 270CW.
            // Future idea: compare the top line of image pixels with the top line of thumbnail pixels.
            std::swap(width, height);

            Orientation imageOrientation = value(EXIF_IFD_0, EXIF_TAG_ORIENTATION).toInt();
            orientation = imageOrientation.isRotated() ? imageOrientation : Orientation::Rotate270CW;
        }

        return Pics::fromImageReader(&reader, width, height, orientation);
    }

    if ((type & Thumbnail::ScaledJpeg) && !mFileName.isEmpty())
    {
        QImageReader reader(mFileName);
        Orientation orientation = value(EXIF_IFD_0, EXIF_TAG_ORIENTATION).toInt();

        // fix non-rotated image
        QSize size = reader.size();
        if (orientation == Orientation::Unknown && ((mWidth > mHeight) != (size.width() > size.height())))
        {
            std::swap(width, height);
            orientation = Orientation::Rotate270CW;
        }

        return Pics::fromImageReader(&reader, width, height, orientation);
    }

    return {};
}

ExifData* File::data() const
{
    return mExifData;
}

ExifContent *File::content(ExifIfd ifd) const
{
    return mExifData ? mExifData->ifd[ifd] : nullptr;
}

ExifEntry *File::entry(ExifIfd ifd, ExifTag tag) const
{
    return exif_content_get_entry(content(ifd), tag);
}

Orientation File::orientation() const
{
    QVariant val = value(EXIF_IFD_0, EXIF_TAG_ORIENTATION);
    return val.isNull() ? Orientation::Unknown : val.toInt();
}

} // namespace Exif
