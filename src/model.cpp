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
    return L.path == R.path && L.pixBase64 == R.pixBase64 && L.position == R.position;
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
    if (!index.isValid() || mData[index.internalId()] == value.toInt())
        return false;

    mData[index.internalId()] = value.toInt();
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

QString IFileListModel::path(const QModelIndex& index)
{
    return index.data(FilePathRole).toString();
}

QStringList IFileListModel::path(const QModelIndexList& indexes)
{
    QStringList list;
    for (const auto& i: indexes)
    {
        QString p = path(i);
        if (!list.contains(p))
        {
            list.append(p);
        }
    }
    return list;
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

    if (role == Qt::DecorationRole && index.column() == 0 && !isDir(index))
        if (auto photo = ExifStorage::data(filePath(index)))
            return Pics::createIcon(photo->pix32, photo->pix16);

    return Super::data(index, role);
}

bool FileTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    return role == Qt::CheckStateRole ?
               setCheckState(index, value) && emitItemChecked(index, value) :
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

QModelIndex FileTreeModel::index(const QString& path) const
{
    return Super::index(path);
}

bool FileTreeModel::emitItemChecked(const QModelIndex& index, const QVariant& value)
{
    Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
    for (const QString& entry: entryList(filePath(index)))
        /*emit*/ itemChecked(entry, state == Qt::Checked);
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

    for (const auto& subdir: subdirs)
    {
        all.append(subdir);
        all.append(entryList(directory.absoluteFilePath(subdir), nameFilters));
    }

    for (const auto& file: files)
        all.append(directory.absoluteFilePath(file));    

    return all;
}

QVariant PhotoListModel::data(const QModelIndex& index, int role) const
{
    if (role == FilePathRole)
        return Super::data(index, Qt::DisplayRole);
    return Super::data(index, role);
}

QModelIndex PhotoListModel::index(const QString& data) const
{
    int row = stringList().indexOf(data);
    return row == -1 ? QModelIndex() : Super::index(row);
}

void PhotoListModel::insert(const QString& line)
{
    auto sl = stringList();
    if (sl.contains(line))
        return;

    int row = 0;
    while (row < sl.size() && line > sl[row])
        ++row;

    insertRow(row);
    setData(index(row), line);
}

void PhotoListModel::remove(const QString& line)
{
    int row = stringList().indexOf(line);
    if (row != -1)
        removeRow(row);
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

    const Bucket& bucket = mBuckets.at(index.row());

    if (role == Role::Pixmap)
        return bucket.photos.size() == 1 ? bucket.photos.first()->pixBase64 : mBubbles.bubble(bucket.photos.size());

    if (role == Role::Path)
        return bucket.photos.size() ? QVariant(bucket.photos.first()->path) : QVariant();

    if (role == Role::Files)
        return bucket.files();

    if (role == Role::Latitude)
        return bucket.position.x();

    if (role == Role::Longitude)
        return bucket.position.y();

    return {};
}

QHash<int, QByteArray> MapPhotoListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Role::Pixmap] = "_pixmap_";
    roles[Role::Path] = "_path_";
    roles[Role::Files] = "_files_";
    roles[Role::Latitude] = "_latitude_";
    roles[Role::Longitude] = "_longitude_";
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
    {
        mBuckets.remove(photo->path);
        mBuckets.insert(photo, mZoom);
    }
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

QModelIndex MapPhotoListModel::index(const QString& path) const
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

void MapPhotoListModel::updateBuckets()
{
    BucketList buckets;
    for (const Bucket& b: mBuckets)
        for (const auto& p: b.photos)
            buckets.insert(p, mZoom);

    if (buckets != mBuckets)
    {
        mBuckets.updateFrom(buckets);
        emit updated();
    }
}

MapPhotoListModel::Bucket::Bucket(const QSharedPointer<Photo> &photo)
{
    insert(photo);
}

bool MapPhotoListModel::Bucket::insert(const QSharedPointer<Photo>& photo)
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
                if (photos.size())
                {
                    if (photos.size() == 1)
                        position = photos.front()->position;
                    else
                        position = pos / photos.size();
                }
                return true;
            }
        }
    }

    return false;
}

bool MapPhotoListModel::Bucket::isValid(const QSharedPointer<Photo> &photo)
{
    return photo && !photo->path.isEmpty() && !photo->pixBase64.isEmpty() && !photo->position.isNull();
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
            if (!bucket.insert(photo))
                return false;

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

void MapSelectionModel::setCurrentRow(int row)
{    
    if (row != currentIndex().row())
        setCurrentIndex(model()->index(row, 0), Clear | Current | Select);
}

void MapSelectionModel::setHoveredRow(int row)
{
    mHoveredRow = row;
}

int MapSelectionModel::currentRow() const
{
    return currentIndex().row();
}

int MapSelectionModel::howeredRow() const
{
    return mHoveredRow;
}

int CoordEditModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : mData.size();
}

int CoordEditModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : COLUMNS_COUNT;
}

QModelIndex CoordEditModel::index(int row, int column, const QModelIndex& parent) const
{
    return parent.isValid() ? QModelIndex() : createIndex(row, column);
}

QModelIndex CoordEditModel::index(const QString& path) const
{
    for (int row = 0; row < mData.size(); ++row)
        if (mData[row].path == path)
            return index(row, 0);

    return {};
}

QModelIndex CoordEditModel::parent(const QModelIndex& /*index*/) const
{
    return {};
}

QVariant CoordEditModel::data(const QModelIndex& index, int role) const
{
    if (index.isValid() &&
        index.row() < rowCount(index.parent()) &&
        index.column() < columnCount(index.parent()))
    {
        const Data& d = mData.at(index.row());
        if (role == Qt::DisplayRole || role == Qt::EditRole)
        {
            if (index.column() == COLUMN_NAME)
                return d.name;
            if (index.column() == COLUMN_POSITION)
                return d.position;
        }

        if (role == IFileListModel::FilePathRole)
        {
            return d.path;
        }
    }

    return {};

}

bool CoordEditModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.isValid() && index.row() < rowCount(index.parent()))
    {
        Data& d = mData[index.row()];

        if (role == Qt::EditRole)
        {
            switch (index.column())
            {
            case COLUMN_POSITION:
                d.position = value.toPointF();
                emit dataChanged(index, index, { role });
                return true;
            }
        }
    }

    return false;
}

bool CoordEditModel::insertRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid()) return false;
    beginInsertRows(parent, row, row + count - 1);
    mData.insert(row, count, {});
    endInsertRows();
    return true;
}

bool CoordEditModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (!parent.isValid() && row >= 0 && row + count - 1 < mData.size())
    {
        beginRemoveRows(parent, row, row + count - 1);
        mData.remove(row, count);
        endRemoveRows();
        return true;
    }

    return false;
}

QVariant CoordEditModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case COLUMN_NAME:
            return tr("Name");
        case COLUMN_POSITION:
            return tr("Coords");
        }
    }

    return {};
}

void CoordEditModel::backup(const QString& path, const QPointF& position)
{
    if (!mBackup.contains(path))
        mBackup[path] = position;
}

void CoordEditModel::update(const QString& path, const QPointF& position)
{
    QModelIndex i = index(path);
    if (i.isValid())
    {
        setData(i.siblingAtColumn(COLUMN_POSITION), position);
        return;
    }

    int row = rowCount();
    beginInsertRows({}, row, row);
    mData.append({ path, QFileInfo(path).fileName(), position });
    endInsertRows();
}

void CoordEditModel::remove(const QString& path)
{
    removeRow(index(path).row());
    mBackup.remove(path);
}

void CoordEditModel::clear()
{
    beginResetModel();
    mData.clear();
    mBackup.clear();
    endResetModel();
}

QStringList CoordEditModel::updated() const
{
    QStringList l;
    l.reserve(mData.size());
    for (const Data& d: mData)
        l.append(d.path);
    return l;
}
