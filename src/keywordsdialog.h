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

    void clear();
    QModelIndex add(const QString& keyword, int count = 0, Qt::ItemFlags extraFlags = Qt::NoItemFlags);
    void setExtraFlags(Qt::ItemFlags flags);

    QStringList values(Qt::CheckState state) const;
    void setChecked(const QSet<QString>& checked, const QSet<QString>& partiallyChecked);

private:
    struct Data
    {
        QString keyword;
        Qt::CheckState checkState = Qt::Unchecked;
        int count = 0;
        Qt::ItemFlags extraFlags = Qt::NoItemFlags;
    };

    QVector<Data> mData;
};


class KeywordsDialog : public QDialog
{
    Q_OBJECT

signals:
    void checkChanged(const QString& keyword, Qt::CheckState state);
    void apply();

public:
    explicit KeywordsDialog(QWidget *parent = nullptr);

    enum class Mode { Filter, Edit };
    void setMode(Mode mode);
    Mode mode() const { return mMode; }

    QTreeView* view() const { return mView; }
    KeywordsModel* model() const { return mModel; }

    enum class Button { Add, Apply };
    QPushButton* button(Button button);

private:
    QTreeView* mView = nullptr;
    KeywordsModel* mModel = nullptr;

    QPushButton* mAdd = nullptr;
    QPushButton* mApply = nullptr;

    Mode mMode = Mode::Edit;
};

#endif // KEYWORDSDIALOG_H
