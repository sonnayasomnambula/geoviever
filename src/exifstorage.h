#ifndef EXIFSTORAGE_H
#define EXIFSTORAGE_H

#include <QObject>
#include <QMap>
#include <QMutex>
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
    QString pixmap; // base64 thumbnail
};

bool operator ==(const ExifData& L, const ExifData& R);
bool operator !=(const ExifData& L, const ExifData& R);

Q_DECLARE_METATYPE(QSharedPointer<Photo>)

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

public:
    static ExifStorage* instance();
    static void destroy();

    static QSharedPointer<Photo> data(const QString& path);
    Q_DECL_DEPRECATED static QPointF coords(const QString& path);
    static QStringList byKeyword(const QString& keyword);

private:
    ExifStorage();
   ~ExifStorage() override;
    void add(const QSharedPointer<Photo>& photo);
    void fail(const QString& path);

    static ExifStorage init();

    QThread mThread;
    mutable QMutex mMutex;
    QMap<QString, QSharedPointer<Photo>> mData;
    QMap<QString, QSet<QString>> mKeywords;
    QSet<QString> mInProgress;
};

#endif // EXIFSTORAGE_H
