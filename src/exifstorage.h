#ifndef EXIFSTORAGE_H
#define EXIFSTORAGE_H

#include <QMap>
#include <QObject>
#include <QPixmap>
#include <QPointF>
#include <QThread>
#include <QStringList>
#include <QWaitCondition>

#include "exif/file.h"

#include "threadsafe.hpp"

struct Photo
{
    QString path;
    QPointF position;
    Exif::Orientation orientation;
    QString keywords;
    QPixmap pix16, pix32;
    QString pixBase64;
};

bool operator ==(const ExifData& L, const ExifData& R);
bool operator !=(const ExifData& L, const ExifData& R);

Q_DECLARE_METATYPE(QSharedPointer<Photo>)


using ThreadSafeStringSet = ThreadSafeSet<QString>;
using ThreadSafePhotoHash = ThreadSafeHash<QString, QWeakPointer<Photo>>;


class WaitCondition : private QWaitCondition
{
    using Super = QWaitCondition;
    QMutex mMutex;

public:
    class Locker
    {
        WaitCondition* mCondition;

    public:
        Locker(WaitCondition* condition);
       ~Locker();
    };

    using Super::wakeOne;
};


class ExifReader : public QThread
{
    Q_OBJECT

signals:
    void ready(const QSharedPointer<Photo>& photo, int loadState);
    void noop();

public:
    enum Loaded
    {
        LoadedEssential = 1, // position, orientation & keywords
        LoadedThumbnail = 2, // embedded or generated thumbnail
    };

    Q_DECLARE_FLAGS(LoadState, Loaded)
    Q_FLAG(LoadState)

    void parse(const QString& path);
    void parseThumbnail(const QSharedPointer<Photo>& photo);

    explicit ExifReader(WaitCondition* condition)
        : mCondition(condition) {}
    void run() override;
    void stop() { mTerminated = true; }

    static int thumbnailSize;

    ThreadSafeStringSet mPending;
    ThreadSafePhotoHash mThumbnailPending;

    WaitCondition* mCondition;
    bool mTerminated = false;
};

class ExifStorage : public QObject
{
    Q_OBJECT

signals:
    void ready(const QSharedPointer<Photo>& photo);
    void remains(int full, int partially);
    void keywordAdded(const QString& keyword, int count);

public:
    enum class Logic { And, Or };

    static ExifStorage* instance();
    static void destroy();

    static void parse(const QString& path);
    static void cancel(const QString& path);

    static QSharedPointer<Photo> data(const QString& path);

    static QStringList keywords();
    static QStringList keywords(const QString& file);
    static QSet<QString> byKeywords(const QStringList& keywords, Logic logic);
    static int count(const QString& keyword);

private:
    ExifStorage();
   ~ExifStorage() override;
    void add(const QSharedPointer<Photo>& photo, int loadState);

    ExifReader mThread;
    ThreadSafeStringSet mPending;
    ThreadSafeStringSet mThumbnailPending;

    WaitCondition mCondition;

    QMutex mMutex;
    QMap<QString, QSharedPointer<Photo>> mData;
    QMap<QString, QSet<QString>> mKeywords;

};

#endif // EXIFSTORAGE_H
