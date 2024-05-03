#ifndef MODEL_H
#define MODEL_H

#include <QFileSystemModel>
#include <QGeoCoordinate>
#include <QMap>
#include <QMutex>
#include <QPersistentModelIndex>
#include <QSharedPointer>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QThread>
#include <QVector>

#include "exif/file.h"

struct Photo
{
    QString path;
    QPointF position;
    Exif::Orientation orientation;
    QString pixmap; // base64 thumbnail
};

Q_DECLARE_METATYPE(QSharedPointer<Photo>)

bool operator ==(const Photo& L, const Photo& R);
bool operator !=(const Photo& L, const Photo& R);


class Bubbles
{
    QHash<int, QString> mData;
    const int mSize;
    const QColor mColor;

public:
    Bubbles(int size, const QColor& color);
    QString bubble(int value);
    static QPixmap generate(int value, int size, const QColor& color);
};


class Checker
{
protected:
    Qt::ItemFlags flags(const QModelIndex& index) const;
    QVariant checkState(const QModelIndex& index) const;
    bool  setCheckState(const QModelIndex& index, const QVariant& value);
    void updateChildrenCheckState(const QModelIndex& index);

private:
    QMap<quintptr, int> mData;
};

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
    static QPointF coords(const QString& path);

private:
    ExifStorage();
    void add(const QSharedPointer<Photo>& photo);
    void fail(const QString& path);

    static ExifStorage init();

    QThread mThread;
    mutable QMutex mMutex;
    QMap<QString, QSharedPointer<Photo>> mData;
    QSet<QString> mInProgress;
};

class FileTreeModel : public QFileSystemModel, public Checker
{
    Q_OBJECT

signals:
    void inserted(const QString& path);
    void removed(const QString& path);

public:
    enum Columns { COLUMN_NAME, COLUMN_COORDS, COLUMNS_COUNT };

    explicit FileTreeModel(QObject *parent = nullptr);
    int columnCount(const QModelIndex& parent) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    static const QStringList entryList(const QString& dir, const QStringList& nameFilters);

private:
    using Super = QFileSystemModel;

    bool setCheckState(const QModelIndex& index, const QVariant& value);
    QStringList entryList(const QString& dir) const;
};

class MapPhotoListModel : public QAbstractListModel
{
    Q_OBJECT

    // TODO extract zoom & center to avoid qml -> cpp -> qml signal loop
    Q_PROPERTY(qreal zoom MEMBER mZoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(QGeoCoordinate center MEMBER mCenter WRITE setCenter NOTIFY centerChanged)
    Q_PROPERTY(int currentRow MEMBER mCurrentRow WRITE setCurrentRow NOTIFY currentRowChanged)
    Q_PROPERTY(int hoveredRow MEMBER mHoveredRow WRITE setHoveredRow)
    Q_PROPERTY(int thumbnailSize MEMBER THUMBNAIL_SIZE CONSTANT)

signals:
    void zoomChanged();
    void centerChanged();
    void currentRowChanged(int row);

public:
    struct Role { enum { Index = Qt::UserRole, Latitude, Longitude, Pixmap, Files }; };

    MapPhotoListModel();

    Q_INVOKABLE int rowCount(const QModelIndex& index = {}) const override;
    Q_INVOKABLE QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Q_INVOKABLE QHash<int, QByteArray> roleNames() const override;

    void insert(const QString& path);
    void remove(const QString& path);
    void update(const QSharedPointer<Photo> &photo);

    void setZoom(qreal zoom);
    void setCenter(const QGeoCoordinate& center);
    void setCenter(const QPointF& center);

    using QAbstractListModel::index;
    QModelIndex index(const QString& path);

    void setCurrentRow(int row);
    void setHoveredRow(int row);

    static constexpr int THUMBNAIL_SIZE = 32;

private:
    void updateBuckets();

    struct Bucket
    {
        QVector<QSharedPointer<Photo>> photos;
        QPointF position;

        Bucket() = default;
        Bucket(const QSharedPointer<Photo>& photo);

        bool insert(const QSharedPointer<Photo>& photo);
        bool remove(const QString& path);
        static bool isValid(const QSharedPointer<Photo>& photo);

        QStringList files() const;

        bool operator ==(const Bucket& other);
        bool operator !=(const Bucket& other);
    };

    class BucketList : private QList<Bucket>
    {
        MapPhotoListModel* mModel = nullptr;
    public:
        BucketList(MapPhotoListModel* model = nullptr) : mModel(model) {}

        bool insert(const QSharedPointer<Photo>& photo, double zoom);
        bool remove(const QString& path);
        void updateFrom(const BucketList& other);

        bool operator ==(const BucketList& other) const;
        bool operator !=(const BucketList& other) const;

        using QList<Bucket>::size;
        using QList<Bucket>::at;
        using QList<Bucket>::begin;
        using QList<Bucket>::end;
    };

    QStringList mKeys;
    BucketList mBuckets;
    mutable Bubbles mBubbles;

    qreal mZoom = 5;
    QGeoCoordinate mCenter;

    int mCurrentRow = -1;
    int mHoveredRow = -1;
};

#endif // MODEL_H
