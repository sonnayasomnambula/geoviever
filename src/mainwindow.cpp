#include <QFileDialog>
#include <QFileSystemModel>
#include <QGeoCoordinate>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

#include "abstractsettings.h"
#include "model.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

struct Settings : AbstractSettings
{
    struct {
        Tag<QString> root = "dirs/root";
    } dirs;

    Tag<QString> filter = "filter";

    struct {
        State state = "window/state";
        Geometry geometry = "window/geometry";
        struct { State state = "window/verticalSplitter.state"; } verticalSplitter;
        struct { State state = "window/horizontalSplitter.state"; } horizontalSplitter;
        struct { State state = "window/header.state"; } header;
    } window;
};

class GeoCoordinateDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QString displayText(const QVariant& value, const QLocale& /*locale*/) const override {
        if (!value.isValid()) return "";
        auto p = value.toPointF();
        return QGeoCoordinate(p.x(), p.y()).toString(QGeoCoordinate::DegreesWithHemisphere);
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mModel(new Model(this))
{
    ui->setupUi(this);
    ui->tree->setItemDelegateForColumn(Model::COLUMN_COORDS, new GeoCoordinateDelegate(this));
    ui->tree->setModel(mModel);
    connect(ui->tree->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this]{
        ui->picture->setPath(mModel->filePath(ui->tree->currentIndex()));
    });
    loadSettings();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* /*e*/)
{
    saveSettings();
}

void MainWindow::loadSettings()
{
    Settings settings;

    settings.window.state.restore(this);
    settings.window.geometry.restore(this);
    settings.window.horizontalSplitter.state.restore(ui->horizontalSplitter);
    settings.window.verticalSplitter.state.restore(ui->verticalSplitter);
    settings.window.header.state.restore(ui->tree->header());

    ui->root->setText(settings.dirs.root(""));
    ui->filter->setText(settings.filter("*.jpg;*.jpeg"));
}

void MainWindow::saveSettings()
{
    Settings settings;

    settings.window.state.save(this);
    settings.window.geometry.save(this);
    settings.window.horizontalSplitter.state.save(ui->horizontalSplitter);
    settings.window.verticalSplitter.state.save(ui->verticalSplitter);
    settings.window.header.state.save(ui->tree->header());

    settings.dirs.root = ui->root->text();
    settings.filter = ui->filter->text();
}

void MainWindow::on_pickRoot_clicked()
{
    Settings settings;

    QString root = settings.dirs.root;
    root = QFileDialog::getExistingDirectory(this, tr("Select root path"), root);
    if (root.isEmpty())
        return;

    ui->root->setText(root);
}

void MainWindow::on_root_textChanged(const QString& text)
{
    mModel->setRootPath(text);
    ui->tree->setRootIndex(mModel->index(text));
}


void MainWindow::on_filter_textChanged(const QString& text)
{
    mModel->setNameFilters(text.split(";"));
    mModel->setNameFilterDisables(false);
}
