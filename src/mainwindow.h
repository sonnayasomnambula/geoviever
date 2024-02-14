#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}

class QFileSystemModel;
class QSortFilterProxyModel;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    void loadSettings();
    void saveSettings();

private slots:
    void on_pickRoot_clicked();
    void on_root_textChanged(const QString &text);
    void on_filter_textChanged(const QString &text);

private:
    Ui::MainWindow* ui = nullptr;
    QFileSystemModel* mModel = nullptr;
};

#endif // MAINWINDOW_H
