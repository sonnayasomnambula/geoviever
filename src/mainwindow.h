#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QImage>
#include <QItemDelegate>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}

class QComboBox;
class QItemSelection;
class QItemSelectionModel;
class QStringListModel;
QT_END_NAMESPACE

class CoordEditDialog;
class FileTreeModel;
class KeywordsDialog;
class PhotoListModel;
class MapPhotoListModel;
class MapSelectionModel;

/// combobox item with [x] button
class ItemButtonDelegate : public QItemDelegate
{
    using Super = QItemDelegate;

    Q_OBJECT

signals:
    void buttonPressed(int index);

public:
    ItemButtonDelegate(const QImage& buttonImage, QComboBox* parent);
    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    bool editorEvent(QEvent* event,
                     QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
private:
    QComboBox* mCombo = nullptr;
    QImage mImage;

    mutable int mButtonSize = 0;
    int mHovered = -1;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* e) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    void loadSettings();
    void saveSettings();

    void showMapTooltip(const QPoint& pos);
    void showTooltip(const QPoint& pos, QAbstractItemView* view);
    bool mapClick(const QMouseEvent* e);
    bool mapMouseMove(const QMouseEvent* e);

    class CursorIcon
    {
        QWidget* mWidget = nullptr;
        Qt::CursorShape mCursorShape = Qt::ArrowCursor;

    public:
        void setWidget(QWidget* w) { mWidget = w; }
        void setCursor(Qt::CursorShape cursorShape) {
            if (cursorShape == mCursorShape)
                return;
            mCursorShape = cursorShape;
            if (mWidget)
                mWidget->setCursor(mCursorShape);
        }

    } mMapCursor;

    QString mPointed; // A picture without coordinates, the coordinates for which will be selected by clicking on the map
    QString mDragged; // A picture in the process of dragging on the map

    QStringList history() const;
    void setHistory(const QStringList& history);

    enum class CreateOption { Never, IfNotExists };
    CoordEditDialog* coordEditDialog(CreateOption createOption = CreateOption::IfNotExists);
    void saveCoords();
    void revertCoords();
    KeywordsDialog* keywordsDialog(CreateOption createOption = CreateOption::IfNotExists);
    void keywordsChanged();
    void updateKeywordsDialog(const QStringList& selectedFiles);
    void saveKeywords();

    void updatePicture(const QString& path);

    void syncSelection();
    void applySelection(QAbstractItemView* to, const QStringList& selectedFiles);
    void applySelection(QItemSelectionModel* to, const QStringList& selectedFiles);

    void syncCurrentIndex(const QModelIndex& currentIndex);
    void applyCurrentIndex(QAbstractItemView* to, const QString& path);
    void applyCurrentIndex(QItemSelectionModel* to, const QString& path, QAbstractItemView* view = nullptr);

    QAbstractItemView* currentView() const;
    QModelIndexList currentSelection() const;

private slots:
    void on_pickRoot_clicked();
    void on_keywords_clicked();
    void on_root_currentTextChanged(const QString& text);
    void on_filter_textChanged(const QString& text);
    void on_tree_doubleClicked(const QModelIndex& index);

    void on_actionCheck_triggered();
    void on_actionUncheck_triggered();
    void on_actionUncheckAll_triggered();
    void on_actionEditKeywords_toggled(bool checked);
    void on_actionEditCoords_toggled(bool checked);
    void on_actionIconView_toggled(bool toggled);

    void on_actionCopyKeywords_triggered();
    void on_actionPasteKeywords_triggered();
    void on_actionCopyCoords_triggered();
    void on_actionPasteCoords_triggered();
    void on_actionProxySave_triggered();

private:
    Ui::MainWindow* ui = nullptr;
    FileTreeModel*    mTreeModel = nullptr;
    PhotoListModel* mCheckedModel = nullptr;
    MapPhotoListModel* mMapModel = nullptr;
    MapSelectionModel* mMapSelectionModel = nullptr;

    QMap<QItemSelectionModel*, QModelIndexList> mSelection;
    QMap<QItemSelectionModel*, QModelIndex> mCurrentIndex;
};

#endif // MAINWINDOW_H
