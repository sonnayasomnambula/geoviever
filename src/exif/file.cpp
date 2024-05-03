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

class FileHelper
{
public:
    static void log(ExifLog* /*log*/, ExifLogCode code, const char* domain, const char* format, va_list args, void* self)
    {
        constexpr size_t size = 512;
        char buffer[size];
        vsnprintf(buffer, size, format, args);

        QString& message = reinterpret_cast<File*>(self)->mErrorString;
        message = QString("[%1] %2").arg(domain).arg(buffer);

        (code == EXIF_LOG_CODE_DEBUG ? qDebug() : qWarning()).noquote() << message;
    }

    template <typename T>
    static T getter(const unsigned char* buf, ExifByteOrder order);

    template <typename T>
    static T extract(ExifData* data, ExifIfd ifd, ExifTag tag, ExifByteOrder order, const T notset)
    {
        if (auto content = data ? data->ifd[ifd] : nullptr)
            if (auto entry = exif_content_get_entry(content, tag))
                if (entry->components == 1 && entry->size == sizeof(notset)) // TODO you must check entry->format
                    return getter<T>(entry->data, order);
        return notset;
    }
};

template <>
ExifShort FileHelper::getter(const unsigned char* buf, ExifByteOrder order) {
    return exif_get_short(buf, order);
}

template <>
ExifLong FileHelper::getter(const unsigned char* buf, ExifByteOrder order) {
    return exif_get_long(buf, order);
}


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

    void* memory;
    size_t size = static_cast<size_t>(ascii.size());
    if (size && *ascii.rbegin())
        ++size; // add 1 for the '\0' terminator (supported by QByteArray, see the docs)
    ExifEntry *entry = exif_content_get_entry(mExifData->ifd[ifd], tag);

    if (entry)
    {
        if (entry->size == size)
        {
            memcpy(entry->data, ascii.data(), size);
            return;
        }
        else
        {
            memory = exif_mem_realloc(mAllocator, entry->data, size);
        }
    }
    else
    {
        entry = exif_entry_new_mem(mAllocator);
        memory = exif_mem_alloc(mAllocator, size);
    }

    memcpy(memory, ascii.data(), size);

    entry->data = static_cast<unsigned char*>(memory);
    entry->size = size;
    entry->tag = tag;
    entry->components = entry->size;
    entry->format = EXIF_FORMAT_ASCII;

    exif_content_add_entry(mExifData->ifd[ifd], entry); // Attach the ExifEntry to an IFD
    exif_entry_unref(entry);
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

uint16_t File::int16u(ExifIfd ifd, ExifTag tag, uint16_t notset) const
{
    return FileHelper::extract(mExifData, ifd, tag, exif_data_get_byte_order(mExifData), notset);
}

uint32_t File::int32u(ExifIfd ifd, ExifTag tag, uint32_t notset) const
{
    return FileHelper::extract(mExifData, ifd, tag, exif_data_get_byte_order(mExifData), notset);
}

QPixmap File::thumbnail(int width, int height) const
{
    Orientation orientation = int16u(EXIF_IFD_1, EXIF_TAG_ORIENTATION, Orientation::Unknown);

    if (mExifData && mExifData->data && mExifData->size)
    {
        QByteArray data = QByteArray::fromRawData(reinterpret_cast<const char*>(mExifData->data), mExifData->size); // not copied
        QBuffer buffer(&data);
        QImageReader reader(&buffer);

        // fix non-rotated EXIF thumbnail
        QSize size = reader.size();
        if (orientation == Orientation::Unknown && ((mWidth > mHeight) != (size.width() > size.height())))
        {
            std::swap(width, height);
            orientation = Orientation::Rotate270CW;
        }

        return Pics::fromImageReader(&reader, width, height, orientation);
    }

    if (!mFileName.isEmpty())
    {
        QImageReader reader(mFileName);
        return Pics::fromImageReader(&reader, width, height, orientation);
    }

    return {};
}

Orientation File::orientation() const
{
    return static_cast<Orientation>(int16u(EXIF_IFD_0, EXIF_TAG_ORIENTATION, static_cast<uint16_t>(Orientation::Unknown)));
}

} // namespace Exif
