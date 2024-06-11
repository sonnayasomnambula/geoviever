#include <QBuffer>
#include <QDebug>
#include <QFileSystemModel>
#include <QGeoCoordinate>
#include <QModelIndex>
#include <QPainter>
#include <QPixmap>
#include <QThread>

#include <cmath>

#include "exif/file.h"
#include "exif/utils.h"

#include "exifstorage.h"
#include "model.h"
#include "pics.h"

bool operator ==(const Photo& L, const Photo& R)
{
    return L.path == R.path && L.pixmap == R.pixmap && L.position == R.position;
}

bool operator !=(const Photo& L, const Photo& R)
{
    return !(L == R);
}

QModelIndexList Checker::children(const QAbstractItemModel* model, Qt::CheckState state, const QModelIndex& parent)
{
    QModelIndexList list;
    for (int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex i = model->index(r, 0, parent);
        if (i.data(Qt::CheckStateRole).toInt() == state)
            list.append(i);
        if (model->rowCount(i))
            list.append(children(model, state, i));
    }
    return list;
}

Qt::ItemFlags Checker::flags(const QModelIndex& index) const
{
    return index.column() == 0 ? Qt::ItemIsUserCheckable : Qt::NoItemFlags;
}

QVariant Checker::checkState(const QModelIndex& index) const
{
    if (!index.isValid() || index.column() != 0)
        return {};

    QModelIndex current = index;
    while (current.isValid())
    {
        auto i = mData.constFind(current.internalId());
        if (i != mData.constEnd())
            return *i;
        current = current.parent();
    }

    return Qt::Unchecked;
}

bool Checker::setCheckState(const QModelIndex& index, const QVariant& value)
{
    if (!index.isValid())
        return false;

    mData.insert(index.internalId(), value.toInt());
    emit const_cast<QAbstractItemModel*>(index.model())->dataChanged(index, index, { Qt::CheckStateRole });
    updateChildrenCheckState(index);
    return true;
}

void Checker::updateChildrenCheckState(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    auto model = const_cast<QAbstractItemModel*>(index.model());
    int rowCount = model->rowCount(index);
    QModelIndexList children;
    children.reserve(rowCount);
    for (int row = 0; row < rowCount; ++row)
        children.append(model->index(row, 0, index));
    if (!children.isEmpty())
    {
        for (const auto& child: children)
            mData.remove(child.internalId());

        emit model->dataChanged(children.first(), children.last(), { Qt::CheckStateRole  });

        for (const auto& child: children)
            updateChildrenCheckState(child);
    }
}

FileTreeModel::FileTreeModel(QObject *parent)
    : Super(parent)
{
    qDebug() << "main thread ID is" << QThread::currentThreadId();

    connect(ExifStorage::instance(), &ExifStorage::ready, this, [this](const QSharedPointer<Photo>& photo){
        QModelIndex i = index(photo->path);
        if (i.isValid()) {
            emit dataChanged(i.siblingAtColumn(COLUMN_COORDS), i.siblingAtColumn(COLUMN_KEYWORDS), { Qt::DisplayRole });
        }
    });
}

int FileTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
    return COLUMNS_COUNT;
}

Qt::ItemFlags FileTreeModel::flags(const QModelIndex& index) const
{
    return Super::flags(index) | Checker::flags(index);
}

QVariant FileTreeModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::CheckStateRole)
        return Checker::checkState(index);

    if ((role == Qt::DisplayRole || role == Qt::EditRole) && index.column() != COLUMN_NAME)
    {
        if (isDir(index))
            return {};

        if (auto photo = ExifStorage::data(filePath(index)))
        {
            switch (index.column())
            {
            case COLUMN_COORDS:
                return photo->position;
            case COLUMN_KEYWORDS:
                return photo->keywords;
            }
        }

        return {};
    }

    return Super::data(index, role);
}

bool FileTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    return role == Qt::CheckStateRole ?
               Checker::setCheckState(index, value) && FileTreeModel::setCheckState(index, value) :
               Super::setData(index, value, role);
}

QVariant FileTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case COLUMN_COORDS:
            return tr("Coords");
        case COLUMN_KEYWORDS:
            return tr("Keywords");
        }
    }

    return Super::headerData(section, orientation, role);
}

bool FileTreeModel::setCheckState(const QModelIndex& index, const QVariant& value)
{
    if (!index.isValid())
        return false;

    Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
    for (const QString& entry: entryList(filePath(index)))
        /*emit*/ state == Qt::Checked ? inserted(entry) : removed(entry);
    return true;
}

QStringList FileTreeModel::entryList(const QString& dir) const
{
    return entryList(dir, nameFilters());
}

const QStringList FileTreeModel::entryList(const QString &dir, const QStringList& nameFilters)
{
    if (QFileInfo(dir).isFile())
        return { dir };

    QDir directory(dir);

    auto files = directory.entryList(nameFilters, QDir::Files, QDir::Name);
    auto subdirs = directory.entryList({}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    QStringList all;

    for (const auto& file: files)
        all.append(directory.absoluteFilePath(file));

    for (const auto& subdir: subdirs)
        all.append(entryList(directory.absoluteFilePath(subdir), nameFilters));

    return all;
}

// QML-used objects must be destoyed after QML engine so don't pass parent here
MapPhotoListModel::MapPhotoListModel() : mBuckets(this), mBubbles(THUMBNAIL_SIZE, Qt::darkBlue)
{
    ExifReader::thumbnailSize = THUMBNAIL_SIZE;

    connect(this, &MapPhotoListModel::zoomChanged, this, &MapPhotoListModel::updateBuckets);
}

int MapPhotoListModel::rowCount(const QModelIndex& index) const
{
    return index.isValid() ? 0 : mBuckets.size();
}

QVariant MapPhotoListModel::data(const QModelIndex& index, int role) const
{
    bool ok = index.isValid() && index.row() < rowCount();
    if (!ok)
        return {};

    int row = index.row();

    if (role == Role::Index)
        return row;

    const Bucket& bucket = mBuckets.at(row);

    if (role == Role::Latitude)
        return bucket.position.x();

    if (role == Role::Longitude)
        return bucket.position.y();

    if (role == Role::Pixmap)
        return bucket.photos.size() == 1 ? bucket.photos.first()->pixmap : mBubbles.bubble(bucket.photos.size());

    if (role == Role::Files)
        return bucket.files();

    return {};
}

QHash<int, QByteArray> MapPhotoListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Role::Index] = "_index_";
    roles[Role::Latitude] = "_latitude_";
    roles[Role::Longitude] = "_longitude_";
    roles[Role::Pixmap] = "_pixmap_";
    roles[Role::Files] = "_files_";
    return roles;
}

void MapPhotoListModel::clear()
{
    mKeys.clear();
    mBuckets.clear();
}

void MapPhotoListModel::insert(const QString& path)
{
    mKeys.append(path);
    if (auto photo = ExifStorage::data(path))
        mBuckets.insert(photo, mZoom);
}

void MapPhotoListModel::remove(const QString& path)
{
    int i = mKeys.indexOf(path);
    if (i != -1)
    {
        mKeys.removeAt(i); // TODO remove data?
        mBuckets.remove(path);
    }
}

void MapPhotoListModel::update(const QSharedPointer<Photo>& photo)
{
    if (mKeys.contains(photo->path))
        mBuckets.insert(photo, mZoom);
}

void MapPhotoListModel::setZoom(qreal zoom)
{
    if (!qFuzzyCompare(zoom, mZoom)) {
        mZoom = zoom;
        emit zoomChanged();
    }
}

void MapPhotoListModel::setCenter(const QGeoCoordinate& center)
{
    if (center != mCenter)
    {
        mCenter = center;
        emit centerChanged();
    }
}

void MapPhotoListModel::setCenter(const QPointF& center)
{
    setCenter(QGeoCoordinate(center.x(), center.y()));
}

QModelIndex MapPhotoListModel::index(const QString& path)
{
    for (int row = 0; row < mBuckets.size(); ++row)
    {
        for (const auto& photo: mBuckets.at(row).photos)
        {
            if (photo && photo->path == path)
            {
                return index(row, 0);
            }
        }
    }

    return {};
}

void MapPhotoListModel::setCurrentRow(int row)
{
    if (row != mCurrentRow)
    {
        mCurrentRow = row;
        emit currentRowChanged(mCurrentRow);
    }
}

void MapPhotoListModel::setHoveredRow(int row)
{
    if (row != mHoveredRow)
        mHoveredRow = row;
}

void MapPhotoListModel::updateBuckets()
{
    BucketList buckets;
    for (const Bucket& b: mBuckets)
        for (const auto& p: b.photos)
            buckets.insert(p, mZoom);

    if (buckets != mBuckets)
    {
        mBuckets.updateFrom(buckets);
        setCurrentRow(-1);
        setHoveredRow(-1);
    }
}

MapPhotoListModel::Bucket::Bucket(const QSharedPointer<Photo> &photo)
{
    insert(photo);
}

bool MapPhotoListModel::Bucket::insert(const QSharedPointer<Photo> &photo)
{
    if (!isValid(photo))
        return false;

    for (const auto& item: photos)
        if (item->path == photo->path)
            return false;

    position *= photos.size();
    photos.append(photo);
    position += photo->position;
    position /= photos.size();

    return true;
}

bool MapPhotoListModel::Bucket::remove(const QString& path)
{
    for (int i = 0; i < photos.size(); ++i)
    {
        if (const auto& photo = photos[i])
        {
            if (photo->path == path)
            {
                QPointF pos = position * photos.size();
                pos -= photo->position;
                photos.removeAt(i);
                pos /= photos.size();
                return true;
            }
        }
    }

    return false;
}

bool MapPhotoListModel::Bucket::isValid(const QSharedPointer<Photo> &photo)
{
    return photo && !photo->path.isEmpty() && !photo->pixmap.isEmpty() && !photo->position.isNull();
}

QStringList MapPhotoListModel::Bucket::files() const
{
    QStringList list;
    list.reserve(photos.size());
    for (const auto& photo: photos)
        list.append(photo->path);
    return list;
}

bool MapPhotoListModel::Bucket::operator ==(const Bucket& other)
{
    return photos == other.photos;
}

bool MapPhotoListModel::Bucket::operator !=(const Bucket& other)
{
    return photos != other.photos;
}

Bubbles::Bubbles(int size, const QColor& color) : mSize(size), mColor(color)
{

}

QString Bubbles::bubble(int value)
{
    auto i = mData.constFind(value);
    if (i != mData.constEnd())
        return *i;

    QString generated = Pics::toBase64(generate(value, mSize, mColor), "PNG");
    mData.insert(value, generated);
    return generated;
}

QPixmap Bubbles::generate(int value, int size, const QColor& color)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QRect rect = pix.rect().adjusted(1, 1, -1, -1);
    QPainter painter(&pix);
    painter.setPen({ color, 2 });
    painter.setBrush(Qt::white);
    painter.drawEllipse(rect);
    QFont font("Tahoma");
    font.setPixelSize(value >= 100 ? size * 4 / 10 : size / 2);
    painter.setFont(font);
    rect.adjust(-1,-1,0,0);
    painter.drawText(rect, Qt::AlignCenter, QString::number(value));
    return pix;
}

bool MapPhotoListModel::BucketList::insert(const QSharedPointer<Photo>& photo, double zoom)
{
    if (!photo)
        return false;

    QGeoCoordinate position(photo->position.x(), photo->position.y());
    for (int row = 0; row < size(); ++row)
    {
        Bucket& bucket = (*this)[row];
        double dist = QGeoCoordinate(bucket.position.x(), bucket.position.y()).distanceTo(position);

        // https://wiki.openstreetmap.org/wiki/Zoom_levels
        static constexpr double C = 40075016.686 / 2.0;
        double pixel_size = C * std::abs(std::cos(bucket.position.x())) / std::pow(2, zoom + 8);
        double thumb_size = pixel_size * 32; // TODO magic constant
        if (dist < thumb_size)
        {
            bucket.insert(photo);
            if (mModel)
            {
                QModelIndex index = mModel->index(row, 0);
                emit mModel->dataChanged(index, index, { Role::Latitude, Role::Longitude, Role::Pixmap });
            }
            return true;
        }
    }

    if (mModel) mModel->beginInsertRows({}, size(), size());
    append(photo);
    if (mModel) mModel->endInsertRows();

    return true;
}

bool MapPhotoListModel::BucketList::remove(const QString& path)
{
    for (int row = 0; row < size(); ++row)
    {
        Bucket& bucket = (*this)[row];
        if (bucket.remove(path))
        {
            if (bucket.photos.isEmpty())
            {
                if (mModel) mModel->beginRemoveRows({}, row, row);
                removeAt(row);
                if (mModel) mModel->endRemoveRows();
            }
            else
            {
                if (mModel)
                {
                    QModelIndex idx = mModel->index(row);
                    emit mModel->dataChanged(idx, idx, { Role::Latitude, Role::Longitude, Role::Pixmap });
                }
            }

            return true;
        }
    }

    return false;
}

void MapPhotoListModel::BucketList::updateFrom(const BucketList &other)
{
    if (mModel) mModel->beginResetModel();

    QList<Bucket>::clear();
    QList<Bucket>::append(other);

    if (mModel) mModel->endResetModel();
}

void MapPhotoListModel::BucketList::clear()
{
    if (mModel) mModel->beginResetModel();

    QList<Bucket>::clear();

    if (mModel) mModel->endResetModel();
}

bool MapPhotoListModel::BucketList::operator ==(const BucketList& other) const
{
    return QList<Bucket>::operator ==(other);
}

bool MapPhotoListModel::BucketList::operator !=(const BucketList& other) const
{
    return !(*this == other);
}
