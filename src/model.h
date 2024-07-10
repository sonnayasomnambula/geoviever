#ifndef MODEL_H
#define MODEL_H

#include <QFileSystemModel>
#include <QGeoCoordinate>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QVector>

#include "exif/file.h"

struct Photo;


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
public:
    static QModelIndexList children(const QAbstractItemModel* model, Qt::CheckState state, const QModelIndex& parent = {});

protected:
    Qt::ItemFlags flags(const QModelIndex& index) const;
    QVariant checkState(const QModelIndex& index) const;
    bool  setCheckState(const QModelIndex& index, const QVariant& value);
    void updateChildrenCheckState(const QModelIndex& index);

private:
    QMap<quintptr, int> mData;
};


class IFileListModel
{
public:
    enum { FilePathRole = QFileSystemModel::FilePathRole };
    virtual QModelIndex index(const QString& path) const = 0;
    static QString path(const QModelIndex& index);
};


class FileTreeModel : public QFileSystemModel, public Checker, public IFileListModel
{
    using Super = QFileSystemModel;
    Q_OBJECT

signals:
    void itemChecked(const QString& path, bool checked);

public:
    enum Columns { COLUMN_NAME, COLUMN_COORDS, COLUMN_KEYWORDS, COLUMNS_COUNT };

    explicit FileTreeModel(QObject *parent = nullptr);
    int columnCount(const QModelIndex& parent) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(const QString& path) const override;
    using Super::index;

    static const QStringList entryList(const QString& dir, const QStringList& nameFilters);

private:
    bool setCheckState(const QModelIndex& index, const QVariant& value);
    QStringList entryList(const QString& dir) const;
};


class PhotoListModel : public QStringListModel, public IFileListModel
{
    using Super = QStringListModel;
    Q_OBJECT

public:
    using Super::Super;

    QVariant data(const QModelIndex& index, int role) const override;
    QModelIndex index(const QString& data) const override;
    using Super::index;

    void insert(const QString& line);
    void remove(const QString& line);
};

class MapPhotoListModel : public QAbstractListModel, public IFileListModel
{
    Q_OBJECT

    // TODO extract zoom & center to avoid qml -> cpp -> qml signal loop
    Q_PROPERTY(qreal zoom MEMBER mZoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(QGeoCoordinate center MEMBER mCenter WRITE setCenter NOTIFY centerChanged)
    Q_PROPERTY(int thumbnailSize MEMBER THUMBNAIL_SIZE CONSTANT)

signals:
    void zoomChanged();
    void centerChanged();
    void updated();

public:
    struct Role { enum { Pixmap = Qt::DecorationRole, Path = FilePathRole, Files, Latitude, Longitude }; };

    MapPhotoListModel();

    Q_INVOKABLE int rowCount(const QModelIndex& index = {}) const override;
    Q_INVOKABLE QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Q_INVOKABLE QHash<int, QByteArray> roleNames() const override;

    void clear();

    void insert(const QString& path);
    void remove(const QString& path);
    void update(const QSharedPointer<Photo>& data);

    void setZoom(qreal zoom);
    void setCenter(const QGeoCoordinate& center);
    void setCenter(const QPointF& center);

    using QAbstractListModel::index;
    QModelIndex index(const QString& path) const override;

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

        void clear();

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
};

class MapSelectionModel : public QItemSelectionModel
{
    using Super = QItemSelectionModel;
    Q_OBJECT

    Q_PROPERTY(int currentRow WRITE setCurrentRow)
    Q_PROPERTY(int hoveredRow MEMBER mHoveredRow WRITE setHoveredRow)

public:
    using Super::Super;

    void setCurrentRow(int row);
    void setHoveredRow(int row);

    int howeredRow() const;

private:
    int mHoveredRow = -1;
};

#endif // MODEL_H
