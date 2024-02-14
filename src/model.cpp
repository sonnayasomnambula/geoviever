#include <QDebug>
#include <QFileSystemModel>
#include <QGeoCoordinate>
#include <QModelIndex>
#include <QThread>

#include "exif.h"
#include "model.h"

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

void ExifReader::parse(const QStringList& files)
{
    qDebug() << QThread::currentThreadId() << Q_FUNC_INFO;
    for (const QString& file: files)
    {
        Exif::File exif;
        if (!exif.load(QDir::toNativeSeparators(file), false))
        {
            qWarning() << "Unable to load" << QDir::toNativeSeparators(file);
            continue;
        }

        auto lat = exif.uRationalVector(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE);
        auto lon = exif.uRationalVector(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE);
        auto latRef = exif.ascii(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE_REF);
        auto lonRef = exif.ascii(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE_REF);

        if (!lat.isEmpty() && !lon.isEmpty())
        {
            QGeoCoordinate coords = Exif::Utils::fromLatLon(lat, latRef, lon, lonRef);
            qDebug() << file << coords.toString();
            emit ready(file, { coords.latitude(), coords.longitude() });
        }
    }
}

ExifStorage::ExifStorage(QObject* parent) : QObject(parent)
{
    auto reader = new ExifReader;
    reader->moveToThread(&mThread);
    connect(&mThread, &QThread::finished, reader, &QObject::deleteLater);
    connect(this, &ExifStorage::parse, reader, &ExifReader::parse);
    connect(reader, &ExifReader::ready, this, &ExifStorage::ready);
    connect(reader, &ExifReader::ready, this, [this](const QString& file, const QPointF& coords){
        mData[file] = coords;
        emit ready(file);
    });
    mThread.start();
}

ExifStorage::~ExifStorage()
{
    mThread.quit();
    mThread.wait();
}

Model::Model(QObject *parent)
    : Super(parent)
{
    qDebug() << QThread::currentThreadId() << Q_FUNC_INFO;

    connect(this, &Model::dataChanged, this, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles){
        if (roles.contains(Qt::CheckStateRole)) {
            QStringList files;
            for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                QModelIndex i = index(row, 0, topLeft.parent());
                if (bool checked = i.data(Qt::CheckStateRole).toBool()) {
                    Q_UNUSED(checked);
                    QString path = filePath(i);
                    if (isDir(i)) {
                        files.append(entryList(path));
                    } else {
                        if (!mExifStorage.data().contains(path)) {
                            files.append(path);
                        }
                    }
                }
            }
            if (!files.isEmpty()) {
                mExifStorage.parse(files);
            }
        }
    });

    connect(&mExifStorage, &ExifStorage::ready, this, [this](const QString& file){
        QModelIndex i = index(file);
        if (i.isValid()) {
            i = i.siblingAtColumn(COLUMN_COORDS);
            emit dataChanged(i, i, { Qt::DisplayRole });
        }
    });
}

int Model::columnCount(const QModelIndex& /*parent*/) const
{
    return COLUMNS_COUNT;
}

Qt::ItemFlags Model::flags(const QModelIndex& index) const
{
    return Super::flags(index) | Checker::flags(index);
}

QVariant Model::data(const QModelIndex& index, int role) const
{
    if (role == Qt::CheckStateRole)
        return Checker::checkState(index);

    if (index.column() == COLUMN_COORDS && (role == Qt::DisplayRole || role == Qt::EditRole ))
        return mExifStorage.data().value(filePath(index));

    return Super::data(index, role);
}

bool Model::setData(const QModelIndex& index, const QVariant& value, int role)
{
    return role == Qt::CheckStateRole ?
               Checker::setCheckState(index, value) :
               Super::setData(index, value, role);
}

QVariant Model::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section == COLUMN_COORDS && orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return tr("Coords");

    return Super::headerData(section, orientation, role);
}

QStringList Model::entryList(const QString& dir) const
{
    QDir directory(dir);
    auto files = directory.entryList(nameFilters(), QDir::Files, QDir::Name);
    auto subdirs = directory.entryList({}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    QStringList all;

    for (const auto& file: files) {
        QString path = directory.absoluteFilePath(file);
        if (!mExifStorage.data().contains(path)) {
            all.append(path);
        }
    }

    for (const auto& subdir: subdirs) {
        QString path = directory.absoluteFilePath(subdir);
        for (const auto& file: entryList(path)) {
            if (!mExifStorage.data().contains(file)) {
                all.append(file);
            }
        }
    }

    return all;
}
