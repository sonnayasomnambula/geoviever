#include <QBoxLayout>
#include <QDebug>
#include <QHeaderView>
#include <QTreeView>
#include <QPushButton>
#include <QRadioButton>
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
    Qt::ItemFlags extraFlags = index.row() < rowCount(index.parent()) ? mData[index.row()].extraFlags : Qt::NoItemFlags;
    return Super::flags(index) | Qt::ItemIsUserCheckable | extraFlags;
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

    if (role == Qt::EditRole && index.column() == COLUMN_KEYWORD)
    {
        beginResetModel();
        mData[index.row()].checkState = Qt::Checked;
        std::sort(mData.begin(), mData.end(), [](const Data& L, const Data& R){
            return L.keyword.toUpper() < R.keyword.toUpper(); });
        endResetModel();
    }

    return ok;
}

void KeywordsModel::clear()
{
    if (mData.isEmpty()) return;

    beginResetModel();
    mData.clear();
    endResetModel();
}

QModelIndex KeywordsModel::insert(const QString& keyword, int count, Qt::ItemFlags extraFlags)
{    
    auto keywords = values();
    int row = keyword.isEmpty() ? -1 : keywords.indexOf(keyword, Qt::CaseInsensitive);
    if (row != -1) {
        mData[row].count = count;
        mData[row].extraFlags = extraFlags;
        emit dataChanged(index(row, COLUMN_KEYWORD_COUNT), index(row, COLUMN_COUNT));
        return index(row, 0);
    }

    row = rowCount();
    if (!keyword.isEmpty())
    {
        keywords.append(keyword);

        std::sort(keywords.begin(), keywords.end(), [](const QString& L, const QString& R){
            return L.toUpper() < R.toUpper();
        });

        row = keywords.indexOf(keyword);
    }

    beginInsertRows({}, row, row);
    mData.insert(row, { keyword, Qt::Unchecked, count, extraFlags });
    endInsertRows();

    return index(row, 0);
}

void KeywordsModel::setExtraFlags(Qt::ItemFlags flags)
{
    for (Data& data : mData)
        data.extraFlags = flags;
}

QStringList KeywordsModel::values() const
{
    QStringList values;
    for (const Data& data: mData)
        values.append(data.keyword);
    return values;
}

QStringList KeywordsModel::values(Qt::CheckState state) const
{
    QStringList values;
    for (const Data& data: mData)
        if (data.checkState == state)
            values.append(data.keyword);
    return values;
}

void KeywordsModel::setChecked(const QSet<QString>& checked, const QSet<QString>& partiallyChecked)
{
    // I don't think the KeywordsModel will ever become hierarchical
    for (int row = 0; row < rowCount(); ++row)
    {
        // for (int col = 0; col < mModel->columnCount(); ++col)
        const int col = 0;
        {
            QModelIndex idx = index(row, col);
            QString keyword = data(idx).toString();
            setData(idx,
                    checked.contains(keyword) ? Qt::Checked :
                        partiallyChecked.contains(keyword) ? Qt::PartiallyChecked : Qt::Unchecked,
                    Qt::CheckStateRole);
        }
    }
}

void KeywordsDialog::setMode(Mode mode)
{
    if (mode != mMode)
    {
        mMode = mode;

        mView->setColumnHidden(KeywordsModel::COLUMN_KEYWORD_COUNT, mMode == Mode::Edit);

        if (mMode != Mode::Edit) // hide first
        {
            mInsert->hide();
            mApply->hide();
        }

        mOr->setVisible(mMode == Mode::Filter);
        mAnd->setVisible(mMode == Mode::Filter);

        if (mMode == Mode::Edit)
        {
            mInsert->show();
            mApply->show();
        }
    }
}

QAbstractButton* KeywordsDialog::button(Button button)
{
    switch (button)
    {
    case Button::Insert:
        return mInsert;
    case Button::Apply:
        return mApply;
    case Button::Or:
        return mOr;
    case Button::And:
        return mAnd;
    }

    return nullptr;
}

KeywordsDialog::KeywordsDialog(QWidget* parent)
    : QDialog(parent)
    , mView(new QTreeView(this))
    , mModel(new KeywordsModel(this))
    , mInsert(new QPushButton(tr("Insert"), this))
    , mApply(new QPushButton(tr("Apply"), this))
    , mOr(new QRadioButton(tr("OR"), this))
    , mAnd(new QRadioButton(tr("AND"), this))
{
    mView->setModel(mModel);
    mView->setIndentation(0);
    mView->setHeaderHidden(true);
    mView->header()->setResizeContentsPrecision(-1); // does not works...
    mView->header()->setSectionResizeMode(KeywordsModel::COLUMN_KEYWORD, QHeaderView::ResizeToContents); // does not works... (
    mView->header()->setSectionResizeMode(KeywordsModel::COLUMN_KEYWORD_COUNT, QHeaderView::Stretch);
    mView->setItemDelegateForColumn(KeywordsModel::COLUMN_KEYWORD_COUNT, new CountDelegate(this));

    mInsert->setShortcut(Qt::Key_Insert);
    mApply->setShortcut(Qt::Key_F2);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(mModel, &KeywordsModel::dataChanged, this, [this](const QModelIndex& /*topLeft*/,
                                                              const QModelIndex& /*bottomRight*/,
                                                              const QVector<int>& roles){
        if (roles.contains(Qt::CheckStateRole)) {
            emit changed();
            mApply->setEnabled(true); // TODO check ExifStorage::keywords() == model->keywords()
        }

        if (roles.contains(Qt::EditRole)) {
            mApply->setEnabled(true);
        }
    });

    connect(mApply, &QPushButton::clicked, this, &KeywordsDialog::apply);

    connect(mInsert, &QPushButton::clicked, this, [this]{
        mView->edit(model()->insert("", 0, Qt::ItemIsEditable));
    });

    connect(mAnd, &QRadioButton::toggled, this, &KeywordsDialog::changed);

    auto lay = new QVBoxLayout(this);
    auto blay = new QHBoxLayout;

    blay->setContentsMargins(11,6,11,6);
    blay->setSpacing(6);

    lay->setContentsMargins({});
    lay->setSpacing(0);

    blay->addWidget(mInsert);
    blay->addStretch();
    blay->addWidget(mApply);
    blay->addWidget(mOr);
    blay->addWidget(mAnd);

    lay->addWidget(mView);
    lay->addLayout(blay);

    setWindowTitle(tr("Keywords"));

    setMode(Mode::Filter);
    mOr->setChecked(true);
}
