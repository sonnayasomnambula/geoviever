#include <QDir>
#include <QPixmap>
#include <QVariant>

#include "exif/file.h"
#include "exif/utils.h"

#include "exifstorage.h"
#include "pics.h"

int ExifReader::thumbnailSize = 32;

bool ThreadSafeStringSet::insert(const QString& s)
{
    QMutexLocker lock(&mMutex);
    int sz = Super::size();
    Super::insert(s);
    return Super::size() > sz;
}

void ThreadSafeStringSet::remove(const QString& s)
{
    QMutexLocker lock(&mMutex);
    Super::remove(s);
}

void ThreadSafeStringSet::clear()
{
    QMutexLocker lock(&mMutex);
    Super::clear();
}

QString ThreadSafeStringSet::takeFirst()
{
    QMutexLocker lock(&mMutex);
    if (isEmpty()) return "";
    auto first = cbegin();
    QString s = *first;
    erase(first);
    return s;
}

int ThreadSafeStringSet::size() const
{
    QMutexLocker lock(&mMutex);
    return Super::size();
}

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

    if (auto photo = load(path))
        emit ready(photo);
    else
        emit failed(path);
}

void ExifReader::run()
{
    while (!mTerminated)
    {
        WaitCondition::Locker lock(mCondition);

        while (!mTerminated)
        {
            QString path = mPending->takeFirst();
            if (path.isEmpty()) break;
            parse(path);
        }
    }
}

QSharedPointer<Photo> ExifReader::load(const QString& path)
{
    auto data = QSharedPointer<Photo>::create();
    data->path = path;

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
    }

    QPixmap pix = exif.thumbnail(thumbnailSize, thumbnailSize);
    if (!pix.isNull())
    {
        data->pix32 = pix.width() == 32 ? pix : pix.scaled(32, 32, Qt::KeepAspectRatio);
        data->pix16 = pix.scaled(16, 16, Qt::KeepAspectRatio);
        data->pixBase64 = Pics::toBase64(pix, "JPEG");
    }

    return data;
}

ExifStorage::ExifStorage() : mThread(&mPending, &mCondition)
{
    qRegisterMetaType< QSharedPointer<Photo> >();

    connect(&mThread, &ExifReader::ready, this, &ExifStorage::add);
    connect(&mThread, &ExifReader::failed, this, &ExifStorage::fail);

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
    QMap<QString, int> keywords;

    {
        QMutexLocker lock(&mMutex);
        mData[photo->path] = photo;

        if (!photo->keywords.isEmpty())
        {
            for (const QString& keyword: photo->keywords.split(';'))
            {
                QSet<QString>& files = mKeywords[keyword.trimmed()];
                files.insert(photo->path);
                keywords[keyword] = files.size();
            }
        }

        rest = mPending.size();
    }

    emit ready(photo);
    emit remains(rest);
    for (auto i = keywords.cbegin(); i != keywords.cend(); ++i)
        emit keywordAdded(i.key(), i.value());
}

void ExifStorage::fail(const QString& /*path*/)
{
    int rest = 0;

    {
        QMutexLocker lock(&mMutex);
        rest = mPending.size();
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

    storage->mThread.stop();
    storage->mCondition.wakeOne();

    storage->mThread.quit();
    storage->mThread.wait();
}

void ExifStorage::parse(const QString& path)
{
    auto storage = instance();
    if (storage->mPending.insert(path))
        storage->mCondition.wakeOne();
}

void ExifStorage::cancel(const QString& path)
{
    auto storage = instance();
    storage->mPending.remove(path);
}

QSharedPointer<Photo> ExifStorage::data(const QString& path)
{
    if (!path.isEmpty())
    {
        auto storage = instance();
        QMutexLocker lock(&storage->mMutex);
        auto i = storage->mData.constFind(path);
        if (i != storage->mData.constEnd())
            return *i;

        if (storage->mPending.insert(path))
            storage->mCondition.wakeOne();
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
