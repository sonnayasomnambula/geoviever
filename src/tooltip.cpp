#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QScreen>
#include <QScrollBar>

#include <cmath>

#include "exifstorage.h"
#include "model.h"
#include "pics.h"
#include "tooltip.h"

QRect TooltipUtils::adjustedRect(const QPoint& pos, const QSize& size, int shift)
{
    QRect rect(pos + QPoint(shift, shift), size);
    QScreen* screen = QGuiApplication::screenAt(pos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect screenRect = screen->geometry();
    if (rect.right() > screenRect.right())
        rect.moveLeft(screenRect.right() - rect.width());
    if (rect.bottom() > screenRect.bottom())
        rect.moveTop(screenRect.bottom() - rect.height());
    return rect;
}

int GridToolTip::Model::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : mRowCount;
}

int GridToolTip::Model::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : mColCount;
}

QVariant GridToolTip::Model::data(const QModelIndex& index, int role) const
{
    int internalIndex = index.row() * mColCount + index.column();
    if (internalIndex >= mData.size())
        return {};

    if (role == Qt::DecorationRole)
    {
        if (auto photo = ExifStorage::data(mData[internalIndex]))
            return Pics::fromBase64(photo->pixmap);
        else
            return Pics::transparent(ExifReader::thumbnailSize, ExifReader::thumbnailSize);
    }

    if (role == IFileListModel::FilePathRole)
        return mData[internalIndex];

    if (role == Qt::SizeHintRole)
        return QSize(ExifReader::thumbnailSize + 4, ExifReader::thumbnailSize + 4);

    return {};
}

void GridToolTip::Model::setFiles(const QStringList& files)
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

void GridToolTip::moveSelection(int dx, int dy)
{
    auto current = currentIndex();
    auto next = current.sibling(current.row() + dy, current.column() + dx);
    if (next.isValid())
        setCurrentIndex(next);
}

void GridToolTip::showAt(const QPoint& pos, int shift)
{
    // https://stackoverflow.com/a/8771172
    int w = verticalHeader()->width() + 4; // +4 seems to be needed
    for (int i = 0; i < mModel->columnCount(); i++)
        w += columnWidth(i); // seems to include gridline (on my machine)
    int h = horizontalHeader()->height() + 4;
    for (int i = 0; i < mModel->rowCount(); i++)
        h += rowHeight(i);
    if (h > w)
    {
        h = w;
        w += verticalScrollBar()->sizeHint().width();
    }

    QRect rect = adjustedRect(pos, QSize(w, h), shift);

    move(rect.topLeft());
    resize(rect.size());
    show();
}

void GridToolTip::keyPressEvent(QKeyEvent* e)
{
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

    AbstractToolTip::keyPressEvent(e);
}

GridToolTip::GridToolTip(QWidget* parent) : AbstractToolTip(parent)
{
    setModel(mModel);
    horizontalHeader()->hide();
    verticalHeader()->hide();
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setShowGrid(false);
}

void GridToolTip::setFiles(const QStringList& files)
{
    mModel->setFiles(files);
    resizeRowsToContents();
    resizeColumnsToContents();
}

void LabelTooltip::showAt(const QPoint &pos, int shift)
{
    move(adjustedRect(pos, sizeHint(), shift).topLeft());
    show();
}
