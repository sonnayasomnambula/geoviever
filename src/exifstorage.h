#ifndef EXIFSTORAGE_H
#define EXIFSTORAGE_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QPixmap>
#include <QPointF>
#include <QSet>
#include <QThread>
#include <QStringList>
#include <QWaitCondition>

#include "exif/file.h"

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


class ThreadSafeStringSet : private QSet<QString>
{
    using Super = QSet<QString>;
    mutable QMutex mMutex;

public:
    bool insert(const QString& s);
    void remove(const QString& s);

    void clear();
    QString takeFirst();

    int size() const;
};


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
    void ready(const QSharedPointer<Photo>& photo);
    void failed(const QString& path);

public:
    void parse(const QString& path);

    explicit ExifReader(ThreadSafeStringSet* s, WaitCondition* c) : mPending(s), mCondition(c) {}
    void run() override;
    void stop() { mTerminated = true; }

public:
    static QSharedPointer<Photo> load(const QString& path);
    static int thumbnailSize;

    ThreadSafeStringSet* mPending;
    WaitCondition* mCondition;
    bool mTerminated = false;
};

class ExifStorage : public QObject
{
    Q_OBJECT

signals:
    void ready(const QSharedPointer<Photo>& photo);
    void remains(int count);
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
    void add(const QSharedPointer<Photo>& photo);
    void fail(const QString& path);

    ExifReader mThread;
    ThreadSafeStringSet mPending;
    WaitCondition mCondition;

    QMutex mMutex;
    QMap<QString, QSharedPointer<Photo>> mData;
    QMap<QString, QSet<QString>> mKeywords;

};

#endif // EXIFSTORAGE_H
