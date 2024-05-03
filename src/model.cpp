#include <QBuffer>
#include <QDebug>
#include <QFileSystemModel>
#include <QGeoCoordinate>
#include <QModelIndex>
#include <QPainter>
#include <QPixmap>
#include <QImageReader>
#include <QThread>

#include <cmath>

#include "exif/file.h"
#include "exif/utils.h"
#include "qexifimageheader.h"
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

QPointF fromLatLon(const QVector<QExifURational>& lat, const QByteArray& latRef, const QVector<QExifURational>& lon, const QByteArray& lonRef)
{
    if (lat.size() != 3 || lon.size() != 3) {
        qWarning() << "Exif: unsupported latlon format" << lat << latRef << lon << lonRef;
        return {};
    }

    class DMS {
        double d, m, s;
    public:
        explicit DMS(const QVector<QExifURational>& value) :
            d(1.0 * value[0].first / value[0].second),
            m(1.0 * value[1].first / value[1].second),
            s(1.0 * value[2].first / value[2].second)
        {}
        double join() { return d + m / 60 + s / 60 / 60; }
    };

    double llat = DMS(lat).join();
    double llon = DMS(lon).join();

    if (latRef == "S")
        llat = -llat;
    if (lonRef == "W")
        llon = -llon;

    return QPointF(llat, llon);
}

void ExifReader::parse(const QString& file)
{
    if (!sender()) return; // disconnected

    Photo photo;
    if (load(&photo, file))
        emit ready(photo);
}

bool ExifReader::load(Photo* photo, const QString& file)
{
    Exif::File exif;
    if (!exif.load(QDir::toNativeSeparators(file), false))
        return false; // no EXIF here

    photo->path = file;

    auto lat = exif.uRationalVector(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE);
    auto lon = exif.uRationalVector(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE);
    auto latRef = exif.ascii(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE_REF);
    auto lonRef = exif.ascii(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE_REF);

    if (!lat.isEmpty() && !lon.isEmpty())
    {
        QGeoCoordinate coords = Exif::Utils::fromLatLon(lat, latRef, lon, lonRef);
        photo->position = { coords.latitude(), coords.longitude() };
    }

    photo->orientation = exif.orientation();

    QPixmap pix = exif.thumbnail(32, 32); // TODO magic constant
    if (!pix.isNull())
        photo->pixmap = Pics::toBase64(pix, "JPEG");

    return true;
}

// QExifImageHeader class can be used for reading EXIF data
// but writing contains a lot of bugs, so this method is not used
bool ExifReader::load_q(Photo* photo, const QString& file)
{
    QExifImageHeader header;
    if (!header.loadFromJpeg(file))
        return false;

    photo->path = file;

    photo->orientation = static_cast<Exif::Orientation>(header.value(QExifImageHeader::Orientation).toShort());

    auto latTag = header.value(QExifImageHeader::GpsLatitude).toRationalVector();
    auto lonTag = header.value(QExifImageHeader::GpsLongitude).toRationalVector();
    auto latRef = header.value(QExifImageHeader::GpsLatitudeRef).toByteArray();
    auto lonRef = header.value(QExifImageHeader::GpsLongitudeRef).toByteArray();

    if (latTag.size() == 3 && lonTag.size() == 3)
    {
        photo->position = fromLatLon(latTag, latRef, lonTag, lonRef);
    }

    QPixmap pix;
    auto thumbnail = header.thumbnailData();
    if (!thumbnail.data.isEmpty())
    {
        QBuffer buffer(&thumbnail.data);
        QImageReader reader(&buffer);
        auto orientation = static_cast<Exif::Orientation>(thumbnail.orientation);
        pix = Pics::fromImageReader(&reader, 32, 32, orientation == Exif::Orientation::Unknown ? photo->orientation : orientation);
        if (orientation == Exif::Orientation::Unknown && photo->orientation != Exif::Orientation::Unknown)
        {
            QSize size_tmb = reader.size();
            QSize size_img = QImageReader(file).size();

            // fix FastStone batch resized images
            if ((size_img.width() > size_img.height()) != (size_tmb.width() > size_tmb.height()))
                pix = pix.transformed(QTransform().rotate(270));
        }
    }

    if (pix.isNull())
    {
        QImageReader reader(file);
        pix = Pics::fromImageReader(&reader, 32, 32, photo->orientation);
    }

    if (!pix.isNull())
    {
        photo->pixmap = Pics::toBase64(pix, "JPEG");
    }

    return true;
}

ExifStorage::ExifStorage()
{
    auto reader = new ExifReader;
    reader->moveToThread(&mThread);
    connect(&mThread, &QThread::finished, reader, &QObject::deleteLater);
    connect(this, &ExifStorage::parse, reader, &ExifReader::parse);
    connect(reader, &ExifReader::ready, this, &ExifStorage::add);
    mThread.start();
}

void ExifStorage::add(const Photo &photo)
{
    {
        QMutexLocker lock(&mMutex);
        mData[photo.path] = photo;
    }

    emit ready(photo);
}

Photo ExifStorage::dummy(const QString& path)
{
    return { path, {}, Exif::Orientation::Unknown, {} };
}

ExifStorage ExifStorage::init()
{
    qRegisterMetaType<Photo>();
    return {};
}

ExifStorage* ExifStorage::instance()
{
    static ExifStorage storage = init();
    return &storage;
}

void ExifStorage::destroy()
{
    auto storage = instance();
    storage->disconnect();

    storage->mThread.quit();
    storage->mThread.wait();
}

bool ExifStorage::fillData(const QString& path, Photo* photo)
{
    auto storage = instance();
    QMutexLocker lock(&storage->mMutex);
    auto i = storage->mData.constFind(path);
    if (i != storage->mData.constEnd())
    {
        *photo = *i;
        return true;
    }

    if (!storage->mInProgress.contains(path))
    {
        storage->mInProgress.append(path);
        emit storage->parse(path);
    }

    return false;
}

QPointF ExifStorage::coords(const QString& path)
{
    auto storage = instance();
    QMutexLocker lock(&storage->mMutex);
    auto i = storage->mData.constFind(path);
    if (i != storage->mData.constEnd())
        return i->position;

    if (!storage->mInProgress.contains(path))
    {
        storage->mInProgress.append(path);
        emit storage->parse(path);
    }

    return {};
}

FileTreeModel::FileTreeModel(QObject *parent)
    : Super(parent)
{
    qDebug() << "main thread ID is" << QThread::currentThreadId();

    connect(ExifStorage::instance(), &ExifStorage::ready, this, [this](const Photo& photo){
        QModelIndex i = index(photo.path);
        if (i.isValid()) {
            i = i.siblingAtColumn(COLUMN_COORDS);
            emit dataChanged(i, i, { Qt::DisplayRole });
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

    if (index.column() == COLUMN_COORDS && (role == Qt::DisplayRole || role == Qt::EditRole ))
        return isDir(index) ? QVariant() : QVariant(ExifStorage::coords(filePath(index)));

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
    if (section == COLUMN_COORDS && orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return tr("Coords");

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
MapPhotoListModel::MapPhotoListModel() : mBuckets(this), mBubbles(32, Qt::darkBlue)
{
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
        return bucket.photos.size() == 1 ? bucket.photos.first().pixmap : mBubbles.bubble(bucket.photos.size());

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

void MapPhotoListModel::insert(const QString& path)
{
    mKeys.append(path);

    Photo photo;
    if (ExifStorage::fillData(path, &photo))
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

void MapPhotoListModel::update(const Photo& photo)
{
    if (mKeys.contains(photo.path))
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

QModelIndex MapPhotoListModel::index(const QString& path)
{
    for (int row = 0; row < mBuckets.size(); ++row)
    {
        for (const Photo& photo: mBuckets.at(row).photos)
        {
            if (photo.path == path)
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
        for (const Photo& p: b.photos)
            buckets.insert(p, mZoom);

    if (buckets != mBuckets)
    {
        mBuckets.updateFrom(buckets);
        setCurrentRow(-1);
        setHoveredRow(-1);
    }
}

MapPhotoListModel::Bucket::Bucket(const Photo& photo)
{
    insert(photo);
}

bool MapPhotoListModel::Bucket::insert(const Photo& photo)
{
    if (!isValid(photo))
        return false;

    for (const Photo& item: photos)
        if (item.path == photo.path)
            return false;

    position *= photos.size();
    photos.append(photo);
    position += photo.position;
    position /= photos.size();

    return true;
}

bool MapPhotoListModel::Bucket::remove(const QString& path)
{
    for (int i = 0; i < photos.size(); ++i)
    {
        if (photos[i].path == path)
        {
            QPointF pos = position * photos.size();
            pos -= photos[i].position;
            photos.removeAt(i);
            pos /= photos.size();
            return true;
        }
    }

    return false;
}

bool MapPhotoListModel::Bucket::isValid(const Photo& photo)
{
    return !photo.path.isEmpty() && !photo.pixmap.isEmpty() && !photo.position.isNull();
}

QStringList MapPhotoListModel::Bucket::files() const
{
    QStringList list;
    list.reserve(photos.size());
    for (const Photo& photo: photos)
    list.append(photo.path);
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

int MapPhotoListModel::BucketList::insert(const Photo& photo, double zoom)
{
    QGeoCoordinate position(photo.position.x(), photo.position.y());
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
            return row;
        }
    }

    if (mModel) mModel->beginInsertRows({}, size(), size());
    append(photo);
    if (mModel) mModel->endInsertRows();

    return size();
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

bool MapPhotoListModel::BucketList::operator ==(const BucketList& other) const
{
    return QList<Bucket>::operator ==(other);
}

bool MapPhotoListModel::BucketList::operator !=(const BucketList& other) const
{
    return !(*this == other);
}
