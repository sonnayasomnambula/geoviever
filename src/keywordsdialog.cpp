#include <QBoxLayout>
#include <QDebug>
#include <QHeaderView>
#include <QTreeView>
#include <QPushButton>
#include <QStyledItemDelegate>

#include "keywordsdialog.h"


class CountDelegate : public QStyledItemDelegate
{
    using Super = QStyledItemDelegate;
public:
    using Super::Super;
    QString displayText(const QVariant& value,
                        const QLocale& /*locale*/) const override {
        if (int count = value.toInt())
            return QString::number(count);
        return "";
    }
    void initStyleOption(QStyleOptionViewItem* option,
                         const QModelIndex& index) const override {
        Super::initStyleOption(option, index);
        option->displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
        option->palette.setColor(QPalette::Active, QPalette::Text, option->palette.color(QPalette::Disabled, QPalette::Text));
    }
};


int KeywordsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : mData.size();
}

int KeywordsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

QModelIndex KeywordsModel::index(int row, int column, const QModelIndex& parent) const
{
    return parent.isValid() ? QModelIndex() : createIndex(row, column);
}

QModelIndex KeywordsModel::parent(const QModelIndex& /*index*/) const
{
    return {};
}

Qt::ItemFlags KeywordsModel::flags(const QModelIndex& index) const
{
    return Super::flags(index) | Qt::ItemIsUserCheckable;
}

QVariant KeywordsModel::data(const QModelIndex& index, int role) const
{
    if (index.isValid() && index.row() < mData.size())
    {
        const Data & data = mData[index.row()];
        if ((role == Qt::DisplayRole || role == Qt::EditRole) && index.column() == COLUMN_KEYWORD)
            return data.keyword;
        if ((role == Qt::DisplayRole || role == Qt::EditRole) && index.column() == COLUMN_KEYWORD_COUNT)
            return data.count;
        if (role == Qt::CheckStateRole && index.column() == 0)
            return data.checkState;
    }

    return {};
}

bool KeywordsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    bool ok = false;

    if (index.isValid() && index.row() < mData.size())
    {
        if ((role == Qt::DisplayRole || role == Qt::EditRole) && index.column() == COLUMN_KEYWORD)
            mData[index.row()].keyword = value.toString(), ok = true;
        if ((role == Qt::DisplayRole || role == Qt::EditRole) && index.column() == COLUMN_KEYWORD_COUNT)
            mData[index.row()].count = value.toInt(), ok = true;
        if (role == Qt::CheckStateRole && index.column() == 0)
            mData[index.row()].checkState = static_cast<Qt::CheckState>(value.toInt()), ok = true;
    }

    if (ok)
        emit dataChanged(index, index, { role });
    return ok;
}

void KeywordsModel::add(const QString &keyword, int count)
{
    beginInsertRows({}, rowCount(), rowCount());
    mData.append({ keyword, Qt::Unchecked, count });
    endInsertRows();
}

void KeywordsModel::clear()
{
    if (mData.isEmpty()) return;

    beginResetModel();
    mData.clear();
    endResetModel();
}

QStringList KeywordsModel::values(Qt::CheckState state) const
{
    QStringList values;
    for (const Data& data: mData)
        if (data.checkState == state)
            values.append(data.keyword);
    return values;
}

KeywordsDialog::KeywordsDialog(QWidget* parent)
    : QDialog(parent)
    , mView(new QTreeView(this))
    , mModel(new KeywordsModel(this))
    , mAdd(new QPushButton(tr("Add"), this))
    , mApply(new QPushButton(tr("Apply"), this))
{
    mView->setModel(mModel);
    mView->setIndentation(0);
    mView->setHeaderHidden(true);
    mView->header()->setResizeContentsPrecision(-1); // does not works...
    mView->header()->setSectionResizeMode(KeywordsModel::COLUMN_KEYWORD, QHeaderView::ResizeToContents); // does not works... (
    mView->header()->setSectionResizeMode(KeywordsModel::COLUMN_KEYWORD_COUNT, QHeaderView::Stretch);
    mView->setItemDelegateForColumn(KeywordsModel::COLUMN_KEYWORD_COUNT, new CountDelegate(this));

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(mModel, &KeywordsModel::dataChanged, this, [this](const QModelIndex& topLeft,
                                                              const QModelIndex& bottomRight,
                                                              const QVector<int>& roles){
        if (roles.contains(Qt::CheckStateRole)) {
            const int col = 0;
            for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                QModelIndex kid = mModel->index(row, col);
                QString keyword = mModel->data(kid, Qt::DisplayRole).toString();
                auto checkState = mModel->data(kid, Qt::CheckStateRole).toInt();
                emit checkChanged(keyword, static_cast<Qt::CheckState>(checkState));
            }
        }
    });

    auto lay = new QVBoxLayout(this);
    auto blay = new QHBoxLayout;

    blay->setContentsMargins(11,6,11,6);
    blay->setSpacing(6);

    lay->setContentsMargins({});
    lay->setSpacing(0);

    blay->addWidget(mAdd);
    blay->addStretch();
    blay->addWidget(mApply);

    lay->addWidget(mView);
    lay->addLayout(blay);

    mAdd->hide();
    mApply->hide();

    setWindowTitle(tr("Keywords"));
}

/*
void KeywordsDialog::setChecked(const QStringList& keywords)
{
    // I don't think the KeywordsModel will ever become hierarchical
    for (int row = 0; row < mModel->rowCount(); ++row)
    {
        // for (int col = 0; col < mModel->columnCount(); ++col)
        const int col = 0;
        {
            QModelIndex idx = mModel->index(row, col);
            QString keyword = mModel->data(idx).toString();
            mModel->setData(idx, keywords.contains(keyword) ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
        }
    }
}
*/
