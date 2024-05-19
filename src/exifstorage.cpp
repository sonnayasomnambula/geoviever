#include <QDir>
#include <QPixmap>
#include <QVariant>

#include "exif/file.h"
#include "exif/utils.h"

#include "exifstorage.h"
#include "pics.h"

int ExifReader::thumbnailSize = 32;

void ExifReader::parse(const QString& path)
{
    if (!sender()) return; // disconnected

    if (auto photo = load(path))
        emit ready(photo);
    else
        emit failed(path);
}

QSharedPointer<Photo> ExifReader::load(const QString& path)
{
    Exif::File exif;
    if (!exif.load(QDir::toNativeSeparators(path), false))
        return {}; // no EXIF here

    auto data = QSharedPointer<Photo>::create();
    data->path = path;

    auto latVal = exif.value(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE);
    auto lonVal = exif.value(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE);
    auto latRef = exif.value(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE_REF).toByteArray();
    auto lonRef = exif.value(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE_REF).toByteArray();

    if (!latVal.isNull() && !lonVal.isNull())
        data->position = Exif::Utils::fromLatLon(latVal.toList(), latRef, lonVal.toList(), lonRef);

    data->orientation = exif.orientation();
    data->keywords = exif.value(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS).toString();

    QPixmap pix = exif.thumbnail(thumbnailSize, thumbnailSize);
    if (!pix.isNull())
        data->pixmap = Pics::toBase64(pix, "JPEG");

    return data;
}

ExifStorage::ExifStorage()
{
    qRegisterMetaType< QSharedPointer<Photo> >();

    auto reader = new ExifReader;
    reader->moveToThread(&mThread);
    connect(&mThread, &QThread::finished, reader, &QObject::deleteLater);
    connect(this, &ExifStorage::parse, reader, &ExifReader::parse);
    connect(reader, &ExifReader::ready, this, &ExifStorage::add);
    connect(reader, &ExifReader::failed, this, &ExifStorage::fail);
    mThread.start();
}

ExifStorage::~ExifStorage()
{
    Q_ASSERT_X(mThread.isFinished(), Q_FUNC_INFO, "You must call ExifStorage::destroy() in main thread before quit (e.g. in QMainWindow::closeEvent)");
    if (!mThread.isFinished())
        destroy();
}

void ExifStorage::add(const QSharedPointer<Photo>& photo)
{
    int rest = 0;

    {
        QMutexLocker lock(&mMutex);
        mData[photo->path] = photo;

        for (const QString& keyword: photo->keywords.split(';'))
            mKeywords[keyword.trimmed()].insert(photo->path);

        mInProgress.remove(photo->path);
        rest = mInProgress.size();
    }

    emit ready(photo);
    emit remains(rest);
}

void ExifStorage::fail(const QString& path)
{
    int rest = 0;

    {
        QMutexLocker lock(&mMutex);
        mInProgress.remove(path);
        rest = mInProgress.size();
    }

    emit remains(rest);
}

ExifStorage* ExifStorage::instance()
{
    static ExifStorage storage;
    return &storage;
}

void ExifStorage::destroy()
{
    auto storage = instance();
    storage->disconnect();

    storage->mThread.quit();
    storage->mThread.wait();
}

QSharedPointer<Photo> ExifStorage::data(const QString& path)
{
    auto storage = instance();
    QMutexLocker lock(&storage->mMutex);
    auto i = storage->mData.constFind(path);
    if (i != storage->mData.constEnd())
        return *i;

    if (!storage->mInProgress.contains(path))
    {
        storage->mInProgress.insert(path);
        emit storage->parse(path);
    }

    return {};
}

QPointF ExifStorage::coords(const QString& path)
{
    auto storage = instance();
    QMutexLocker lock(&storage->mMutex);
    auto i = storage->mData.constFind(path);
    if (i != storage->mData.constEnd())
        return (*i)->position;

    if (!storage->mInProgress.contains(path))
    {
        storage->mInProgress.insert(path);
        emit storage->parse(path);
    }

    return {};
}

QStringList ExifStorage::byKeyword(const QString& keyword)
{
    auto storage = instance();
    QMutexLocker lock(&storage->mMutex);
    return storage->mKeywords.value(keyword).values();
}
