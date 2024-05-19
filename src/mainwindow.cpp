#include <QAbstractTableModel>
#include <QBoxLayout>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QGeoCoordinate>
#include <QImageReader>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QScreen>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QToolTip>

#include <cmath>

#include "exif/file.h"

#include "abstractsettings.h"
#include "exifstorage.h"
#include "model.h"
#include "mainwindow.h"
#include "pics.h"
#include "tooltip.h"
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
    using Super = QStyledItemDelegate;
public:
    using Super::Super;
    QString displayText(const QVariant& value,
                        const QLocale& /*locale*/) const override {
        if (!value.isValid()) return "";
        auto p = value.toPointF();
        return QGeoCoordinate(p.x(), p.y()).toString(QGeoCoordinate::DegreesWithHemisphere);
    }
    void initStyleOption(QStyleOptionViewItem* option,
                         const QModelIndex& index) const override {
        Super::initStyleOption(option, index);
        option->displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mTreeModel(new FileTreeModel(this))
    , mMapModel(new MapPhotoListModel)
{
    ui->setupUi(this);
    ui->tree->setItemDelegateForColumn(FileTreeModel::COLUMN_COORDS, new GeoCoordinateDelegate(this));
    ui->tree->setModel(mTreeModel);
    connect(ui->tree->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& idx){
        if (mTreeModel->isDir(idx))
        {
            ui->picture->setPath("");
            ui->picture->setPixmap({});
            mMapModel->setCurrentRow(-1);
            return;
        }

        QString path = mTreeModel->filePath(idx);
        ui->picture->setPath(path);

        Exif::Orientation orientation;
        if (auto photo = ExifStorage::data(path))
            orientation = photo->orientation;
        else
            orientation = Exif::File(path, false).orientation();

        QImageReader reader(path);
        ui->picture->setPixmap(Pics::fromImageReader(&reader, orientation));

        if (idx.isValid())
        {
            QModelIndex index = mMapModel->index(path);
            mMapModel->setCurrentRow(index.row());
        }
    });
    connect(ui->map, &QQuickWidget::statusChanged, [this](QQuickWidget::Status status){
        if (status == QQuickWidget::Status::Error) {
            // for (const auto& error: ui->map->errors())
                // qWarning() << error.toString();
            qWarning() << "QML load failed";
        }
    });
    connect(ui->map, &QQuickWidget::sceneGraphError, [this](QQuickWindow::SceneGraphError, const QString& message){
        qWarning() << "SceneGraphError" << message;
    });

    connect(mTreeModel, &FileTreeModel::inserted, mMapModel, &MapPhotoListModel::insert);
    connect(mTreeModel, &FileTreeModel::removed, mMapModel, &MapPhotoListModel::remove);
    connect(ExifStorage::instance(), &ExifStorage::ready, mMapModel, &MapPhotoListModel::update);
    connect(ExifStorage::instance(), &ExifStorage::remains, this, [this](int count){
        static QElapsedTimer timer;
        if (count && timer.isValid() && timer.elapsed() < 500)
            return;
        statusBar()->showMessage(count ? QString::number(count) : tr("Ready"), count ? 500 : 5000);
        timer.restart();
    });

    connect(mMapModel, &MapPhotoListModel::currentRowChanged, this, [this](int row){
        QModelIndex index = mMapModel->index(row, 0);
        if (index.isValid()) {
            auto files = mMapModel->data(index, MapPhotoListModel::Role::Files).toStringList();
            selectPicture(files.isEmpty() ? "" : files.first());
        }
    });

    ui->map->installEventFilter(this);

    QQmlEngine* engine = ui->map->engine();
    engine->rootContext()->setContextProperty("controller", mMapModel);
    ui->map->setSource(QUrl("qrc:/map.qml"));
    loadSettings();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* /*e*/)
{
    saveSettings();
    ExifStorage::destroy();
}

bool MainWindow::eventFilter(QObject* o, QEvent* e)
{
    if (o == ui->map && e->type() == QEvent::ToolTip)
        if (auto event = dynamic_cast<QHelpEvent*>(e))
            showTooltip(event->globalPos());
    return QObject::eventFilter(o, e);
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

static QRect getRectToShow(QTableView* t, QAbstractItemModel* m, const QPoint& pos)
{
    // https://stackoverflow.com/a/8771172
    int w = t->verticalHeader()->width() + 4; // +4 seems to be needed
    for (int i = 0; i < m->columnCount(); i++)
        w += t->columnWidth(i); // seems to include gridline (on my machine)
    int h = t->horizontalHeader()->height() + 4;
    for (int i = 0; i < m->rowCount(); i++)
        h += t->rowHeight(i);
    if (h > w)
    {
        h = w;
        w += t->verticalScrollBar()->sizeHint().width();
    }

    QRect rect(pos, QSize(w, h));
    QScreen* screen = QGuiApplication::screenAt(pos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect screenRect = screen->geometry();
    if (rect.right() > screenRect.right())
        rect.moveLeft(screenRect.right() - rect.width());
    if (rect.bottom() > screenRect.bottom())
        rect.moveTop(screenRect.bottom() - rect.height());
    return rect;
}

void MainWindow::showTooltip(const QPoint& pos)
{
    int row = mMapModel->property("hoveredRow").toInt(); // TODO getter
    QModelIndex index = mMapModel->index(row, 0);
    QStringList files = mMapModel->data(index, MapPhotoListModel::Role::Files).toStringList();
    if (files.isEmpty())
        return;

    if (files.size() == 1)
    {
        QToolTip::showText(pos, files.first(), this);
        return;
    }

    static ToolTip* widget = nullptr;
    if (!widget)
    {
        widget = new ToolTip(this);
        connect(widget->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& index){
            selectPicture(widget->model()->data(index, ToolTip::FilePathRole).toString());
        });
        connect(widget, &ToolTip::doubleClicked, this, [this](const QModelIndex& index){
            QString path = widget->model()->data(index, ToolTip::FilePathRole).toString();
            if (auto photo = ExifStorage::data(path)) {
                mMapModel->setZoom(18);
                mMapModel->setCenter(photo->position);
            }
        });
    }

    widget->setFiles(files);

    QRect rect = getRectToShow(widget, widget->model(), pos);

    widget->move(rect.topLeft());
    widget->resize(rect.size());

    widget->show();
    widget->setFocus();
}

void MainWindow::selectPicture(const QString& path)
{
    if (path.isEmpty())
    {
        ui->tree->setCurrentIndex({});
        return;
    }

    auto i = mTreeModel->index(path);
    if (i.isValid())
    {
        ui->tree->setCurrentIndex(i);
        return;
    }

    ui->picture->setPath(path);

    Exif::Orientation orientation;
    if (auto photo = ExifStorage::data(path))
        orientation = photo->orientation;
    else
        orientation = Exif::File(path, false).orientation();

    QImageReader reader(path);
    ui->picture->setPixmap(Pics::fromImageReader(&reader, orientation));
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
    mTreeModel->setRootPath(text);
    ui->tree->setRootIndex(mTreeModel->index(text));
    mMapModel->clear();
}


void MainWindow::on_filter_textChanged(const QString& text)
{
    mTreeModel->setNameFilters(text.split(";"));
    mTreeModel->setNameFilterDisables(false);
}

void MainWindow::on_tree_doubleClicked(const QModelIndex& index)
{
    auto coords = index.siblingAtColumn(FileTreeModel::COLUMN_COORDS).data().toPointF();
    if (!coords.isNull())
    {
        mMapModel->setZoom(18);
        mMapModel->setCenter(coords);
    }
}
