#include <QBoxLayout>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStyledItemDelegate>
#include <QTreeView>

#include "dialog.h"
#include "exifstorage.h"
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

        std::initializer_list<Button> editButtons = { Button::Insert, Button::Apply };
        std::initializer_list<Button> filterButtons = { Button::Or, Button::And };

        if (mMode != Mode::Edit) // hide first
            for (Button b: editButtons)
                button(b)->hide();

        for (Button b: filterButtons)
            button(b)->setVisible(mMode == Mode::Filter);

        if (mMode == Mode::Edit)
            for (Button b: editButtons)
                button(b)->show();
    }
}

QAbstractButton* KeywordsDialog::button(Button button) const
{
    return mButtons.value(button);
}

void KeywordsDialog::setFiles(const QStringList& files)
{
    if (mFiles == files)
        return;

    mFiles = files;

    QSet<QString> all, common, partially;

    for (const QString& path: mFiles) {
        if (QFileInfo(path).isDir()) continue;
        QString keywordsTag;
        if (auto photo = ExifStorage::data(path))
            keywordsTag = photo->keywords;
        else
            keywordsTag = Exif::File(path, false).value(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS).toString();

        QSet<QString> keywords;

        for (QString& s: keywordsTag.split(';'))
            keywords.insert(s.trimmed());

        if (all.isEmpty()) {
            all = common = keywords;
        } else {
            all.unite(keywords);
            common.intersect(keywords);
            partially = all - common;
        }
    }

    model()->setChecked(common, partially);
    button(Button::Apply)->setEnabled(false);
}

void KeywordsDialog::apply()
{
    if (!Dialog::canOverwrite(mFiles.size(), this))
        return;

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList warnings;

    for (const auto& path: mFiles) {
        if (QFileInfo(path).isDir()) continue;
        Exif::File file;
        if (!file.load(path)) {
            warnings.append(tr("Load '%1' failed: %2").arg(path, file.errorString()));
            continue;
        }

        QString keywords = model()->values(Qt::Checked).join(';');
        file.setValue(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS, keywords);

        if (!file.save(path)) {
            warnings.append(tr("Save '%1' failed: %2").arg(path, file.errorString()));
            continue;
        }

        if (QSharedPointer<Photo> photo = ExifStorage::data(path))
        {
            photo->keywords = keywords;
            emit ExifStorage::instance()->ready(photo);
        }
    }

    QGuiApplication::restoreOverrideCursor();

    if (warnings.isEmpty()) {
        button(KeywordsDialog::Button::Apply)->setEnabled(false);
        model()->setExtraFlags(Qt::NoItemFlags); // reset
    } else {
        QMessageBox::warning(this, "", warnings.join("\n"));
    }
}

KeywordsDialog::KeywordsDialog(QWidget* parent)
    : QDialog(parent)
    , mView(new QTreeView(this))
    , mModel(new KeywordsModel(this))
{
    mButtons.insert(Button::Insert, new QPushButton(tr("Insert"), this));
    mButtons.insert(Button::Apply, new QPushButton(tr("Apply"), this));
    mButtons.insert(Button::Or, new QRadioButton(tr("OR"), this));
    mButtons.insert(Button::And, new QRadioButton(tr("AND"), this));

    mView->setModel(mModel);
    mView->setIndentation(0);
    mView->setHeaderHidden(true);
    mView->header()->setResizeContentsPrecision(-1); // does not works...
    mView->header()->setSectionResizeMode(KeywordsModel::COLUMN_KEYWORD, QHeaderView::ResizeToContents); // does not works... (
    mView->header()->setSectionResizeMode(KeywordsModel::COLUMN_KEYWORD_COUNT, QHeaderView::Stretch);
    mView->resizeColumnToContents(KeywordsModel::COLUMN_KEYWORD); // QHeaderView::ResizeMode doesn't seem to work
    mView->resizeColumnToContents(KeywordsModel::COLUMN_KEYWORD_COUNT);
    mView->setItemDelegateForColumn(KeywordsModel::COLUMN_KEYWORD_COUNT, new CountDelegate(this));


    button(Button::Insert)->setShortcut(Qt::Key_Insert);
    button(Button::Apply)->setShortcut(Qt::Key_F2);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(mModel, &KeywordsModel::dataChanged, this, [this](const QModelIndex& /*topLeft*/,
                                                              const QModelIndex& /*bottomRight*/,
                                                              const QVector<int>& roles){
        if (roles.contains(Qt::CheckStateRole)) {
            emit changed();
            button(Button::Apply)->setEnabled(true); // TODO check ExifStorage::keywords() == model->keywords()
        }

        if (roles.contains(Qt::EditRole)) {
            button(Button::Apply)->setEnabled(true);
        }
    });

    connect(button(Button::Apply), &QPushButton::clicked, this, &KeywordsDialog::apply);

    connect(button(Button::Insert), &QPushButton::clicked, this, [this]{
        mView->edit(model()->insert("", 0, Qt::ItemIsEditable));
    });

    connect(button(Button::And), &QRadioButton::toggled, this, &KeywordsDialog::changed);

    auto lay = new QVBoxLayout(this);
    auto blay = new QHBoxLayout;

    blay->setContentsMargins(11,6,11,6);
    blay->setSpacing(6);

    lay->setContentsMargins({});
    lay->setSpacing(0);

    blay->addWidget(button(Button::Insert));
    blay->addStretch();
    blay->addWidget(button(Button::Apply));
    blay->addWidget(button(Button::Or));
    blay->addWidget(button(Button::And));

    lay->addWidget(mView);
    lay->addLayout(blay);

    setWindowTitle(tr("Keywords"));

    setMode(Mode::Filter);
    button(Button::Or)->setChecked(true);
}
