#include <QDebug>
#include <QDir>
#include <QPixmap>
#include <QTime>
#include <QLoggingCategory>
#include <QVariant>

#include "exif/file.h"
#include "exif/utils.h"

#include "exifstorage.h"
#include "pics.h"

// #undef qDebug
// #define qDebug QT_NO_QDEBUG_MACRO

Q_LOGGING_CATEGORY(reader, "ExifReader", QtWarningMsg)
Q_LOGGING_CATEGORY(store, "ExifStorage", QtWarningMsg)

int ExifReader::thumbnailSize = 32;

WaitCondition::Locker::Locker(WaitCondition* condition) : mCondition(condition)
{
    mCondition->mMutex.lock();
    mCondition->wait(&mCondition->mMutex);
}

WaitCondition::Locker::~Locker()
{
    mCondition->mMutex.unlock();
}

void ExifReader::parse(const QString& path)
{
    if (path.isEmpty()) return;
    qCDebug(reader) << __func__ << path;

    QSharedPointer<Photo> data = QSharedPointer<Photo>::create();    
    data->path = path;

    QPixmap pix;
    Exif::File exif;
    if (exif.load(QDir::toNativeSeparators(path), false))
    {
        auto latVal = exif.value(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE);
        auto lonVal = exif.value(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE);
        auto latRef = exif.value(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE_REF).toByteArray();
        auto lonRef = exif.value(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE_REF).toByteArray();

        if (!latVal.isNull() && !lonVal.isNull())
            data->position = Exif::Utils::fromLatLon(latVal.toList(), latRef, lonVal.toList(), lonRef);

        data->orientation = exif.orientation();
        data->keywords = exif.value(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS).toString();

        pix = exif.thumbnail(thumbnailSize, thumbnailSize, Exif::File::Embedded);
    }

    if (pix.isNull())
    {
        // no embedded thumbnail in EXIF
        // it will be retrieved later by resizing the image
        // this is a long operation so it is postponed for later
        qCDebug(reader) << QTime::currentTime().toString(Qt::ISODateWithMs) << path << "no embedded thumbnail; insert in 2nd queue";
        mThumbnailPending.insert(path, data);
        emit ready(data, LoadedEssential);
    }
    else
    {
        // embedded thumbnail found
        qCDebug(reader) << QTime::currentTime().toString(Qt::ISODateWithMs) << path << "embedded thumbnail load";
        data->pix32 = pix.width() == 32 ? pix : pix.scaled(32, 32, Qt::KeepAspectRatio);
        data->pix16 = pix.scaled(16, 16, Qt::KeepAspectRatio);
        data->pixBase64 = Pics::toBase64(pix, "JPEG");
        emit ready(data, LoadedEssential | LoadedThumbnail);
    }
}

void ExifReader::parseThumbnail(const QSharedPointer<Photo>& photo)
{
    qCDebug(reader) << __func__ << photo->path;

    Exif::File exif;
    exif.load(QDir::toNativeSeparators(photo->path), false);

    QPixmap pix = exif.thumbnail(thumbnailSize, thumbnailSize, Exif::File::ScaledJpeg);
    if (pix.isNull())
        return;

    photo->pix32 = pix.width() == 32 ? pix : pix.scaled(32, 32, Qt::KeepAspectRatio);
    photo->pix16 = pix.scaled(16, 16, Qt::KeepAspectRatio);
    photo->pixBase64 = Pics::toBase64(pix, "JPEG");
    qCDebug(reader) << QTime::currentTime().toString(Qt::ISODateWithMs) << photo->path << "thumbnail scaled";
    emit ready(photo, LoadedThumbnail);
}

void ExifReader::run()
{
    while (!mTerminated)
    {
        qCDebug(reader) << "thread goes asleep";
        WaitCondition::Locker lock(mCondition);
        qCDebug(reader) << "wake up thread";

        while (!mTerminated)
        {
            static int i = 0;
            QString path = mPending.takeFirst();
            if (!path.isEmpty())
            {
                qCDebug(reader) << __func__ << ++i << path;
                parse(path);
            }
            else
            {
                if (QSharedPointer<Photo> photo = mThumbnailPending.takeFirst().toStrongRef())
                {
                    qCDebug(reader) << __func__ << ++i << photo->path;
                    parseThumbnail(photo);
                }
                else
                    break;
            }
        }

        emit noop();
    }

    qCDebug(reader) << "thread finished";
}

ExifStorage::ExifStorage() : mThread(&mCondition)
{
    qRegisterMetaType< QSharedPointer<Photo> >();

    connect(&mThread, &ExifReader::ready, this, &ExifStorage::add);
    connect(&mThread, &ExifReader::noop, this, [this]{ emit remains(mPending.size(), mThumbnailPending.size()); });

    mThread.start();
}

ExifStorage::~ExifStorage()
{
    Q_ASSERT_X(mThread.isFinished(), Q_FUNC_INFO, "You must call ExifStorage::destroy() in main thread before quit (e.g. in QMainWindow::closeEvent)");
    if (!mThread.isFinished())
        destroy();
}

void ExifStorage::add(const QSharedPointer<Photo>& photo, int loadState)
{    
    if (loadState & ExifReader::LoadedEssential)
    {
        QMutexLocker lock(&mMutex);
        mData[photo->path] = photo;

        if (!photo->keywords.isEmpty())
        {
            QMap<QString, int> keywords;

            for (QString keyword: photo->keywords.split(';'))
            {
                keyword = keyword.trimmed();
                QSet<QString>& files = mKeywords[keyword];
                files.insert(photo->path);
                keywords[keyword] = files.size();
            }

            for (auto i = keywords.cbegin(); i != keywords.cend(); ++i)
                emit keywordAdded(i.key(), i.value());
        }
    }

    if (loadState != (ExifReader::LoadedEssential | ExifReader::LoadedThumbnail))
    {
        if (loadState & ExifReader::LoadedThumbnail)
            mThumbnailPending.remove(photo->path);
        else
            mThumbnailPending.insert(photo->path);
    }

    mPending.remove(photo->path);

    emit ready(photo);
    emit remains(mPending.size(), mThumbnailPending.size());
    qCDebug(store) << __func__ << photo->path << QFlags<ExifReader::Loaded>(loadState) << mPending.size() << mThumbnailPending.size() << "pending";
}

ExifStorage* ExifStorage::instance()
{
    static ExifStorage storage;
    return &storage;
}

void ExifStorage::destroy()
{
    auto storage = instance();

    storage->mThread.stop();
    storage->mCondition.wakeOne();

    storage->mThread.quit();
    storage->mThread.wait();
}

void ExifStorage::parse(const QString& path)
{
    auto storage = instance();

    {
        QMutexLocker lock(&storage->mMutex);
        if (storage->mData.contains(path))
            return;
    }

    qCDebug(store) << __func__ << path;
    if (storage->mPending.insert(path) && storage->mThread.mPending.insert(path))
    {
// FIXME reorganize project to put object files in separate dirs
#if defined(QT_TESTLIB_LIB)
        QThread::msleep(1);
#endif
        storage->mCondition.wakeOne();
    }
}

void ExifStorage::cancel(const QString& path)
{
    auto storage = instance();
    qCDebug(store) << __func__ << path << storage->mThread.mPending.size() << "/" << storage->mThread.mThumbnailPending.size() << "pending in thread";
    storage->mPending.remove(path);
    storage->mThumbnailPending.remove(path);
    storage->mThread.mThumbnailPending.remove(path);
    storage->mThread.mPending.remove(path);
    qCDebug(store) << __func__ << storage->mThread.mPending.size() << "/" << storage->mThread.mThumbnailPending.size() << "pending in thread";
}

QSharedPointer<Photo> ExifStorage::data(const QString& path)
{
    if (!path.isEmpty())
    {
        auto storage = instance();
        QMutexLocker lock(&storage->mMutex);
        auto i = storage->mData.constFind(path);

        if (i == storage->mData.constEnd())
        {
            if (storage->mPending.insert(path) &&
                storage->mThread.mPending.insert(path))
            {
                qCDebug(store) << __func__ << path << "no data found, full processing;" << storage->mPending.size() << "/" << storage->mThumbnailPending.size() << "pending";
                storage->mCondition.wakeOne();
            }
        }
        else if (i.value()->pix16.isNull())
        {
            if (!storage->mPending.contains(path) &&
                 storage->mThread.mThumbnailPending.insert(path, i.value()) &&
                 storage->mThumbnailPending.insert(path))
            {
                qCDebug(store) << __func__ << path << "no pix found, processing thumbnail;" << storage->mPending.size() << "/" << storage->mThumbnailPending.size() << "pending";
                storage->mCondition.wakeOne();
            }
        }

        if (i != storage->mData.constEnd())
            return *i;
    }

    return {};
}

QStringList ExifStorage::keywords()
{
    auto storage = instance();
    QMutexLocker lock(&storage->mMutex);
    return storage->mKeywords.keys();
}

QStringList ExifStorage::keywords(const QString& file)
{
    QString string;
    if (auto photo = data(file))
        string = photo->keywords;
    else
        string = Exif::File(file, false).value(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS).toString();

    QStringList keywords;
    if (!string.isEmpty())
        for (QString& s: string.split(';'))
            keywords.append(s.trimmed());
    return keywords;
}

QSet<QString> ExifStorage::byKeywords(const QStringList& keywords, Logic logic)
{
    if (keywords.isEmpty())
        return {};

    auto storage = instance();
    QMutexLocker lock(&storage->mMutex);

    auto ret = storage->mKeywords.value(keywords[0]);
    for (int i = 1; i < keywords.size(); ++i)
    {
        auto s = storage->mKeywords.value(keywords[i]);
        if (logic == Logic::And)
            ret.intersect(s);
        else
            ret.unite(s);
    }
    return ret;
}

int ExifStorage::count(const QString& keyword)
{
    auto storage = instance();
    QMutexLocker lock(&storage->mMutex);
    return storage->mKeywords.value(keyword).size();
}
