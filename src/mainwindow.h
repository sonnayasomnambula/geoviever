#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
#include <QImage>
#include <QItemDelegate>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}

class QComboBox;
class QListView;
class QStringListModel;
QT_END_NAMESPACE

class FileTreeModel;
class MapPhotoListModel;

class KeywordsDialog : public QDialog
{
public:
    explicit KeywordsDialog(QWidget *parent = nullptr);

    void setKeywords(const QStringList& keywords);

private:
    QListView* mView = nullptr;
    QStringListModel* mModel = nullptr;
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

    void showTooltip(const QPoint& pos);
    void selectPicture(const QString& path);

    QStringList history() const;
    void setHistory(const QStringList& history);

private slots:
    void on_pickRoot_clicked();
    void on_keywords_clicked();
    void on_root_currentTextChanged(const QString& text);
    void on_filter_textChanged(const QString& text);
    void on_tree_doubleClicked(const QModelIndex& index);

private:
    Ui::MainWindow* ui = nullptr;
    FileTreeModel*    mTreeModel = nullptr;
    MapPhotoListModel* mMapModel = nullptr;
    KeywordsDialog* mKeywordsDialog = nullptr;
};

#endif // MAINWINDOW_H
