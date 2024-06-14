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
class QStringListModel;
QT_END_NAMESPACE

class FileTreeModel;
class KeywordsDialog;
class MapPhotoListModel;

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
    void showTreeTooltip(const QPoint& pos);
    void selectPicture(const QString& path);

    QStringList history() const;
    void setHistory(const QStringList& history);

    enum class CreateOption { Never, IfNotExists };
    KeywordsDialog* keywordsDialog(CreateOption createOption = CreateOption::IfNotExists);
    void keywordChecked(const QString& keyword, Qt::CheckState state);
    void updateKeywordsDialog();
    void saveKeywords();
    void updateSelection(const QModelIndex& idx);

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
    void on_list_doubleClicked(const QModelIndex &index);

private:
    Ui::MainWindow* ui = nullptr;
    FileTreeModel*    mTreeModel = nullptr;
    MapPhotoListModel* mMapModel = nullptr;
};

#endif // MAINWINDOW_H
