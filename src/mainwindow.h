#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}

QT_END_NAMESPACE

class FileTreeModel;
class MapPhotoListModel;

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

private slots:
    void on_pickRoot_clicked();
    void on_root_textChanged(const QString& text);
    void on_filter_textChanged(const QString& text);
    void on_tree_doubleClicked(const QModelIndex& index);

private:
    Ui::MainWindow* ui = nullptr;
    FileTreeModel*    mTreeModel = nullptr;
    MapPhotoListModel* mMapModel = nullptr;
};

#endif // MAINWINDOW_H
