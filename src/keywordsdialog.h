#ifndef KEYWORDSDIALOG_H
#define KEYWORDSDIALOG_H

#include <QAbstractListModel>
#include <QDialog>

#include <tuple>


QT_BEGIN_NAMESPACE
class QTreeView;
QT_END_NAMESPACE


class KeywordsModel : public QAbstractItemModel
{
    using Super = QAbstractItemModel;

    Q_OBJECT

public:
    using Super::Super;

    enum { COLUMN_KEYWORD, COLUMN_KEYWORD_COUNT, COLUMN_COUNT };

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    void add(const QString& keyword, int count);
    void clear();

    QStringList values(Qt::CheckState state) const;

private:
    struct Data
    {
        QString keyword;
        Qt::CheckState checkState = Qt::Unchecked;
        int count = 0;
    };

    QVector<Data> mData;
};


class KeywordsDialog : public QDialog
{
    Q_OBJECT

signals:
    void checkChanged(const QString& keyword, Qt::CheckState state);

public:
    explicit KeywordsDialog(QWidget *parent = nullptr);

    // void setChecked(const QStringList& keywords);

    QTreeView* view() const { return mView; }
    KeywordsModel* model() const { return mModel; }

private:
    QTreeView* mView = nullptr;
    KeywordsModel* mModel = nullptr;

    QPushButton* mAdd = nullptr;
    QPushButton* mApply = nullptr;
};

#endif // KEYWORDSDIALOG_H
