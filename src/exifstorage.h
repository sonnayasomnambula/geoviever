#ifndef EXIFSTORAGE_H
#define EXIFSTORAGE_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QPixmap>
#include <QPointF>
#include <QSet>
#include <QThread>

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


/// thread worker
class ExifReader : public QObject
{
    Q_OBJECT

signals:
    void ready(const QSharedPointer<Photo>& photo);
    void failed(const QString& path);

public slots:
    void parse(const QString& path);

public:
    static QSharedPointer<Photo> load(const QString& path);
    static int thumbnailSize;
};

class ExifStorage : public QObject
{
    Q_OBJECT

signals:
    void parse(const QString& file);
    void ready(const QSharedPointer<Photo>& photo);
    void remains(int count);
    void keywordAdded(const QString& keyword, int count);

public:
    enum class Logic { And, Or };

    static ExifStorage* instance();
    static void destroy();

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

    QThread mThread;
    mutable QMutex mMutex;
    QMap<QString, QSharedPointer<Photo>> mData;
    QMap<QString, QSet<QString>> mKeywords;
    QSet<QString> mInProgress;
};

#endif // EXIFSTORAGE_H
