#include <QHeaderView>
#include <QKeyEvent>

#include <cmath>

#include "exifstorage.h"
#include "pics.h"
#include "tooltip.h"

int ToolTip::Model::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : mRowCount;
}

int ToolTip::Model::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : mColCount;
}

QVariant ToolTip::Model::data(const QModelIndex& index, int role) const
{
    int internalIndex = index.row() * mColCount + index.column();
    if (internalIndex >= mData.size())
        return {};

    if (role == Qt::DecorationRole)
        if (auto photo = ExifStorage::data(mData[internalIndex]))
            return Pics::fromBase64(photo->pixmap);

    if (role == FilePathRole)
        return mData[internalIndex];

    if (role == Qt::SizeHintRole)
        return QSize(ExifReader::thumbnailSize + 4, ExifReader::thumbnailSize + 4);

    return {};
}

void ToolTip::Model::setFiles(const QStringList& files)
{
    if (mData == files)
        return;

    beginResetModel();
    mData = files;

    mRowCount = std::sqrt(1.0 * mData.size());
    mColCount = std::ceil(1.0 * mData.size() / mRowCount);
    Q_ASSERT(mColCount * mRowCount >= mData.size());

    const int MAX_COLS = 25;
    if (mColCount > MAX_COLS)
    {
        mColCount = MAX_COLS;
        mRowCount = std::ceil(1.0 * mData.size() / mColCount);
    }

    endResetModel();
}

void ToolTip::moveSelection(int dx, int dy)
{
    auto current = currentIndex();
    auto next = current.sibling(current.row() + dy, current.column() + dx);
    if (next.isValid())
        setCurrentIndex(next);
}

void ToolTip::enterEvent(QEvent*)
{
    if (mTimerId)
    {
        killTimer(mTimerId);
        mTimerId = 0;
    }
}

void ToolTip::leaveEvent(QEvent*)
{
    mTimerId = startTimer(600);
}

void ToolTip::hideEvent(QHideEvent*)
{
    if (mTimerId)
    {
        killTimer(mTimerId);
        mTimerId = 0;
    }
}

void ToolTip::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape)
        hide();

    if (e->key() == Qt::Key_Left)
        moveSelection(-1, 0);

    if (e->key() == Qt::Key_Right)
        moveSelection(+1, 0);

    if (e->key() == Qt::Key_Up)
        moveSelection(0, -1);

    if (e->key() == Qt::Key_Down)
        moveSelection(0, +1);

    if (e->key() == Qt::Key_PageUp)
        moveSelection(0, -currentIndex().row());

    if (e->key() == Qt::Key_PageDown)
        moveSelection(0, mModel->rowCount() - currentIndex().row() - 1);

    if (e->key() == Qt::Key_Home)
        moveSelection(-currentIndex().column(), 0);

    if (e->key() == Qt::Key_End)
        moveSelection(mModel->columnCount() - currentIndex().column() - 1, 0);
}

void ToolTip::timerEvent(QTimerEvent* e)
{
    if (e->timerId() == mTimerId)
        hide();
}

ToolTip::ToolTip(QWidget* parent) : QTableView(parent)
{
    setWindowFlags(Qt::ToolTip);
    setModel(mModel);
    horizontalHeader()->hide();
    verticalHeader()->hide();
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setShowGrid(false);
}

void ToolTip::setFiles(const QStringList& files)
{
    mModel->setFiles(files);
    resizeRowsToContents();
    resizeColumnsToContents();
}
