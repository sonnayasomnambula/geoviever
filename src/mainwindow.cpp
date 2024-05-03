#include <QAbstractTableModel>
#include <QBoxLayout>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QGeoCoordinate>
#include <QKeyEvent>
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

#include "abstractsettings.h"
#include "model.h"
#include "mainwindow.h"
#include "pics.h"
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
    , mTreeModel(new FileTreeModel(this))
    , mMapModel(new MapPhotoListModel)
{
    ui->setupUi(this);
    ui->tree->setItemDelegateForColumn(FileTreeModel::COLUMN_COORDS, new GeoCoordinateDelegate(this));
    ui->tree->setModel(mTreeModel);
    connect(ui->tree->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this]{
        auto idx = ui->tree->currentIndex();
        if (mTreeModel->isDir(idx))
        {
            ui->picture->setPath("");
            ui->picture->setPixmap({});
            mMapModel->setCurrentRow(-1);
            return;
        }

        QString path = mTreeModel->filePath(idx);
        ui->picture->setPath(path);

        Photo photo;
        if (!ExifStorage::fillData(path, &photo))
        {
            Exif::File exif;
            exif.load(path, false);
            photo.orientation = exif.orientation();
        }

        QImageReader reader(path);
        ui->picture->setPixmap(Pics::fromImageReader(&reader, photo.orientation));

        QModelIndex index = mMapModel->index(path);
        if (index.isValid())
            mMapModel->setCurrentRow(index.row()); // <1>
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

    connect(mMapModel, &MapPhotoListModel::currentRowChanged, this, [this](int row){
        QModelIndex index = mMapModel->index(row, 0);
        if (index.isValid()) {
            auto files = mMapModel->data(index, MapPhotoListModel::Role::Files).toStringList();
            selectPicture(files.size() == 1 ? files.first() : ""); // <1>
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

class ToolTip : public QTableView
{
    int mTimerId = 0;

    class Model : public QAbstractTableModel
    {
        QStringList mData;
        int mRowCount = 0;
        int mColCount = 0;

    public:
        using QAbstractTableModel::QAbstractTableModel;

        int rowCount(const QModelIndex& parent = {}) const override
        {
            return parent.isValid() ? 0 : mRowCount;
        }

        int columnCount(const QModelIndex& parent = {}) const override
        {
            return parent.isValid() ? 0 : mColCount;
        }

        QVariant data(const QModelIndex& index, int role) const override
        {
            int internalIndex = index.row() * mColCount + index.column();
            if (internalIndex >= mData.size())
                return {};

            static Photo photo;

            if (role == Qt::DecorationRole)
                if (ExifStorage::fillData(mData[internalIndex], &photo))
                    return Pics::fromBase64(photo.pixmap);

            if (role == FilePathRole)
                return mData[internalIndex];

            if (role == Qt::SizeHintRole)
                return QSize(32+4, 32+4); // TODO magic constant

            return {};
        }

        void setFiles(const QStringList& files)
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

    } * mModel = new Model(this);

    void moveSelection(int dx, int dy)
    {
        auto current = currentIndex();
        auto next = current.sibling(current.row() + dy, current.column() + dx);
        if (next.isValid())
            setCurrentIndex(next);
    }

protected:
    void enterEvent(QEvent*) override
    {
        if (mTimerId)
        {
            killTimer(mTimerId);
            mTimerId = 0;
        }
    }

    void leaveEvent(QEvent*) override
    {
        mTimerId = startTimer(600);
    }

    void hideEvent(QHideEvent*) override
    {
        if (mTimerId)
        {
            killTimer(mTimerId);
            mTimerId = 0;
        }
    }

    void keyPressEvent(QKeyEvent* e) override
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

    void timerEvent(QTimerEvent* e) override
    {
        if (e->timerId() == mTimerId)
            hide();
    }

public:
    enum { FilePathRole = Qt::UserRole };

    explicit ToolTip(QWidget* parent = nullptr) : QTableView(parent)
    {
        setWindowFlags(Qt::ToolTip);
        setModel(mModel);
        horizontalHeader()->hide();
        verticalHeader()->hide();
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setShowGrid(false);
    }

    void setFiles(const QStringList& files)
    {
        mModel->setFiles(files);
        resizeRowsToContents();
        resizeColumnsToContents();
    }

    QAbstractItemModel* model() const { return mModel; }
};

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
            QSignalBlocker lock(mMapModel); // fix preview disappears the first time you click on the tooltip, see <1>
            selectPicture(widget->model()->data(index, ToolTip::FilePathRole).toString());
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

    Photo photo;
    if (!ExifStorage::fillData(path, &photo))
    {
        Exif::File exif;
        exif.load(path, false);
        photo.orientation = exif.orientation();
    }

    QImageReader reader(path);
    ui->picture->setPixmap(Pics::fromImageReader(&reader, photo.orientation));
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
}


void MainWindow::on_filter_textChanged(const QString& text)
{
    mTreeModel->setNameFilters(text.split(";"));
    mTreeModel->setNameFilterDisables(false);
}
