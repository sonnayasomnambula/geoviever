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

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

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

    QStringList history() const;
    void setHistory(const QStringList& history);

    enum class CreateOption { Never, IfNotExists };
    KeywordsDialog* keywordsDialog(CreateOption createOption = CreateOption::IfNotExists);
    void keywordChecked(const QString& keyword, Qt::CheckState state);
    void updateKeywordsDialog();
    void saveKeywords();

    void updatePicture();

    void syncSelection();
    void applySelection(QAbstractItemView* to);
    void applySelection(QItemSelectionModel* to);

    void syncCurrentIndex(const QModelIndex& currentIndex);
    void applyCurrentIndex(QAbstractItemView* to);
    void applyCurrentIndex(QItemSelectionModel* to, QAbstractItemView* view = nullptr);

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
    void on_actionEditKeywords_triggered(bool checked);
    void on_actionIconView_toggled(bool toggled);

private:
    Ui::MainWindow* ui = nullptr;
    FileTreeModel*    mTreeModel = nullptr;
    PhotoListModel* mCheckedModel = nullptr;
    MapPhotoListModel* mMapModel = nullptr;
    MapSelectionModel* mMapSelectionModel = nullptr;

    QStringList mSelection;
    QString mCurrent;
};

#endif // MAINWINDOW_H
