#ifndef MODEL_H
#define MODEL_H

#include <QFileSystemModel>
#include <QMap>
#include <QPersistentModelIndex>
#include <QSortFilterProxyModel>
#include <QThread>

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

public:
    using QObject::QObject;

signals:
    void ready(const QString& file, const QPointF& coords);

public slots:
    void parse(const QStringList& files);
};

class ExifStorage : public QObject
{
    Q_OBJECT

signals:
    void parse(const QStringList& files);
    void ready(const QString& file);

public:
    ExifStorage(QObject* parent = nullptr);
   ~ExifStorage() override;

    const QMap<QString, QPointF>& data() const { return mData; }

private:
    QThread mThread;
    QMap<QString, QPointF> mData;
};

class Model : public QFileSystemModel, public Checker
{
    Q_OBJECT

public:
    enum Columns { COLUMN_NAME, COLUMN_COORDS, COLUMNS_COUNT };

    explicit Model(QObject *parent = nullptr);
    int columnCount(const QModelIndex& parent) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;


private:
    using Super = QFileSystemModel;

    QStringList entryList(const QString& dir) const;

    ExifStorage mExifStorage;
};


// class HideEmptyDirModel : public QSortFilterProxyModel
// {
// public:
//     HideEmptyDirModel(QFileSystemModel* source, QObject* parent = nullptr);
//     bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

// private:
//     using QSortFilterProxyModel::setSourceModel;
//     int childrenCount(const QModelIndex& parent) const;

// private:
//     QFileSystemModel* mSourceModel = nullptr;
// };

#endif // MODEL_H
