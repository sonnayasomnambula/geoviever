#include <QAbstractTableModel>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QGeoCoordinate>
#include <QImageReader>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QToolTip>

#include <cmath>

#include "exif/file.h"
#include "exif/utils.h"

#include "abstractsettings.h"
#include "coordeditdialog.h"
#include "exifstorage.h"
#include "geocoordinate.h"
#include "keywordsdialog.h"
#include "model.h"
#include "mainwindow.h"
#include "pics.h"
#include "qtcompat.h"
#include "tooltip.h"
#include "ui_mainwindow.h"

struct Settings : AbstractSettings
{
    struct {
        Tag<QString> root = "dirs/root";
        Tag<QStringList> history = "dirs/history";
    } dirs;

    Tag<QString> filter = "filter";
    struct {
        State state = "window/state";
        Geometry geometry = "window/geometry";
        struct { State state = "window/mapSplitter.state"; } mapSplitter;
        struct { State state = "window/treeSplitter.state"; } treeSplitter;
        struct { State state = "window/centralSplitter.state"; } centralSplitter;
        struct { State state = "window/header.state"; } header;
    } window;

    struct {
        Geometry geometry = "keywordDialog/geometry";
        Tag<bool> overwriteSilently = "keywordDialog/overwriteSilently";
        Tag<bool> orLogic = "keywordDialog/orLogic";
    } keywordDialog;

    struct {
        Geometry geometry = "coordEditDialog/geometry";
        struct { State state = "coordEditDialog/header.state"; } header;
    } coordEditDialog;
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

class SLPreviewDelegate : public QStyledItemDelegate
{
    using Super = QStyledItemDelegate;

    // TODO use FileTreeModel with proxy model without any delegate
    QFileSystemModel* mSourceModel = nullptr;

public:
    SLPreviewDelegate(QFileSystemModel* sourceModel, QObject* parent = nullptr) : Super(parent), mSourceModel(sourceModel) {}

    QString displayText(const QVariant& value, const QLocale& /*locale*/) const override {
        QDir dir(value.toString());
        return dir.isAbsolute() ? dir.dirName() : value.toString();
    }

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
        Super::initStyleOption(option, index);
        QString path = IFileListModel::path(index);
        if (auto photo = ExifStorage::data(path))
            option->icon = photo->pix32;
        else
            option->icon = qvariant_cast<QIcon>(mSourceModel->data(mSourceModel->index(path), Qt::DecorationRole));
        if (!option->icon.isNull())
            option->features |= QStyleOptionViewItem::HasDecoration;
    }
};

ItemButtonDelegate::ItemButtonDelegate(const QImage& buttonImage, QComboBox* parent)
    : Super(parent)
    , mCombo(parent)
    , mImage(buttonImage)
{
    mCombo->view()->installEventFilter(this);

    mButtonSize = std::max(mButtonSize, mImage.width());
    mButtonSize = std::max(mButtonSize, mImage.height());
}

void ItemButtonDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = setOptions(index, option);
    mButtonSize = std::max(mButtonSize, opt.rect.height());
    opt.rect.setRight(opt.rect.right() - mButtonSize); // button is on the right

    const bool hovered = index.row() == mHovered;
    if (hovered) {
        opt.state &= (~QStyle::State_Selected); // remove highlighting
        opt.state &= (~QStyle::State_HasFocus); // remove focus rect
    }

    Super::paint(painter, opt, index);

    painter->save();

    // draw button rect

    QRect buttonRect(opt.rect.right() + 1, opt.rect.top(), mButtonSize, mButtonSize);
    QColor buttonColor = hovered ?
                             opt.palette.color(QPalette::Highlight).lighter(210) :
                             opt.palette.color(QPalette::Base);
    painter->setPen(buttonColor);
    painter->setBrush(buttonColor);
    painter->drawRect(buttonRect);

    // draw button picture

    int x = buttonRect.left() + ((buttonRect.width() - mImage.width()) / 2);
    int y = buttonRect.top() + ((buttonRect.height() - mImage.height()) / 2);

    if (!hovered)
        painter->setOpacity(0.3);
    painter->drawImage(x, y, mImage);

    painter->restore();
}

QSize ItemButtonDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = Super::sizeHint(option, index);
    return QSize(std::max(size.width(), mButtonSize), std::max(size.height(), mButtonSize));
}

bool ItemButtonDelegate::editorEvent(QEvent *event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if (mButtonSize && event->type() == QEvent::MouseMove) {
        QMouseEvent* e = static_cast<QMouseEvent*>(event);
        int idx = e->pos().x() < option.rect.right() - mButtonSize ? QModelIndex().row() : index.row();
        if (idx != mHovered) {
            mHovered = idx;
            mCombo->view()->viewport()->repaint();
        }
    }

    if (mButtonSize && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* e = static_cast<QMouseEvent*>(event);
        if (e->buttons() & Qt::LeftButton) {
            int idx = e->pos().x() < option.rect.right() - mButtonSize ? QModelIndex().row() : index.row();
            emit buttonPressed(idx);
            return true;
        }
    }

    return Super::editorEvent(event, model, option, index);
}

bool ItemButtonDelegate::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Hide)
        mHovered = -1;
    return Super::eventFilter(object, event);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mTreeModel(new FileTreeModel(this))
    , mCheckedModel(new PhotoListModel(this))
    , mMapModel(new MapPhotoListModel)
    , mMapSelectionModel(new MapSelectionModel(mMapModel))
{
    ui->setupUi(this);

    mMapCursor.setWidget(ui->map);

    ui->actionSeparator1->setSeparator(true);
    ui->actionSeparator2->setSeparator(true);
    ui->actionSeparator3->setSeparator(true);

    auto viewGroup = new QActionGroup(this);
    viewGroup->addAction(ui->actionIconView);
    viewGroup->addAction(ui->actionTreeView);

    QList<QAction*> actions = { ui->actionCheck, ui->actionUncheck, ui->actionUncheckAll,
                                ui->actionSeparator1,
                                ui->actionEditKeywords, ui->actionEditCoords,
                                ui->actionSeparator2,
                                ui->actionCopyKeywords, ui->actionPasteKeywords,
                                ui->actionCopyCoords, ui->actionPasteCoords,
                                ui->actionSeparator3,
                                ui->actionIconView, ui->actionTreeView };
    ui->tree->addActions(actions);
    ui->list->addActions(actions);

    ui->checked->addActions({ ui->actionUncheck, ui->actionUncheckAll,
                              ui->actionSeparator1,
                              ui->actionEditKeywords, ui->actionEditCoords,
                              ui->actionSeparator2,
                              ui->actionCopyCoords,
                              ui->actionPasteCoords });

    addAction(ui->actionProxySave);

    connect(QGuiApplication::clipboard(), &QClipboard::changed, this, [this](QClipboard::Mode mode){
        if (mode == QClipboard::Clipboard) {
            QString text = QGuiApplication::clipboard()->text();
            ui->actionPasteKeywords->setEnabled(!text.isEmpty());
            ui->actionPasteCoords->setEnabled(GeoCoordinate::fromString(text).isValid());
        }
    });

    auto comboDelegate = new ItemButtonDelegate(QImage(":/cross-small.png"), ui->root);
    ui->root->setItemDelegate(comboDelegate);
    connect(comboDelegate, &ItemButtonDelegate::buttonPressed, ui->root, &QComboBox::removeItem);

    ui->tree->setItemDelegateForColumn(FileTreeModel::COLUMN_COORDS, new GeoCoordinateDelegate(this));
    ui->checked->setItemDelegate(new SLPreviewDelegate(mTreeModel, this));

    ui->tree->setModel(mTreeModel);
    ui->list->setModel(mTreeModel);
    ui->checked->setModel(mCheckedModel);

    connect(ui->tree->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::syncSelection);
    connect(ui->list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::syncSelection);
    connect(ui->checked->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::syncSelection);
    connect(mMapSelectionModel, &MapSelectionModel::selectionChanged, this, &MainWindow::syncSelection);

    connect(ui->tree->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MainWindow::syncCurrentIndex);
    connect(ui->list->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::syncCurrentIndex);
    connect(ui->checked->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::syncCurrentIndex);
    connect(mMapSelectionModel, &MapSelectionModel::currentChanged, this, &MainWindow::syncCurrentIndex);

    connect(mMapModel, &MapPhotoListModel::updated, mMapSelectionModel, &MapSelectionModel::clear);

    connect(ui->list, &QListView::doubleClicked, this, &MainWindow::on_tree_doubleClicked);

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

    /*
    connect(mTreeModel, &FileTreeModel::fileRenamed, this, [this](const QString& path, const QString& oldName, const QString& newName){
        // TODO
    });
    */

    connect(mTreeModel, &FileTreeModel::itemChecked, this, [this](const QString& path, bool checked){
        // getting index fixes wrong tree order
        if (mTreeModel->isDir(mTreeModel->index(path)))
            return;

        if (checked) {
            ExifStorage::parse(path);
            mMapModel->insert(path);
            mCheckedModel->insert(path);
        } else {
            ExifStorage::cancel(path);
            mMapModel->remove(path);
            mCheckedModel->remove(path);
        }
    });

    connect(ExifStorage::instance(), &ExifStorage::ready, mMapModel, &MapPhotoListModel::update);
    connect(ExifStorage::instance(), &ExifStorage::remains, this, [this](int f, int p){
        static QElapsedTimer timer;
        static const int UPDATE_TIMEOUT = 400;
        static const int CLEAR_TIMEOUT = 5000;
        if ((f + p) && timer.isValid() && timer.elapsed() < UPDATE_TIMEOUT)
            return;
        statusBar()->showMessage(f || p ? tr("%1 (%2) file(s) in progress...", nullptr, f + p).arg(f).arg(p) : tr("Ready"), f && p ? UPDATE_TIMEOUT : CLEAR_TIMEOUT);
        timer.restart();
    });

    ui->map->installEventFilter(this);
    ui->tree->installEventFilter(this);
    ui->list->installEventFilter(this);

    QQmlEngine* engine = ui->map->engine();
    engine->rootContext()->setContextProperty("controller", mMapModel);
    engine->rootContext()->setContextProperty("selection", mMapSelectionModel);
    ui->map->setSource(QUrl("qrc:/map.qml"));
    loadSettings();

    ui->tree->selectionModel()->setObjectName("treeSelectionModel");
    ui->list->selectionModel()->setObjectName("listSelectionModel");
    ui->checked->selectionModel()->setObjectName("checkedSelectionModel");
    mMapSelectionModel->setObjectName("mapSelctionModel");

    setWindowTitle(QCoreApplication::applicationName() + " " + QCoreApplication::applicationVersion());

    if (QObject* map = ui->map->rootObject()->findChild<QObject*>("map"))
    {
        QSignalBlocker lock(mMapModel);
        mMapModel->setZoom(map->property("zoomLevel").toDouble());
    }
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
        showMapTooltip(static_cast<QHelpEvent*>(e)->globalPos());
    if (o == ui->tree && e->type() == QEvent::ToolTip)
        showTooltip(static_cast<QHelpEvent*>(e)->globalPos(), ui->tree);
    if (o == ui->list && e->type() == QEvent::ToolTip)
        showTooltip(static_cast<QHelpEvent*>(e)->globalPos(), ui->list);
    if (o == ui->map && e->type() == QEvent::MouseButtonPress)
        mapClick(static_cast<QMouseEvent*>(e));
    if (o == ui->map && e->type() == QEvent::MouseButtonRelease)
        mapClick(static_cast<QMouseEvent*>(e));
    if (o == ui->map && e->type() == QEvent::MouseMove)
        return mapMouseMove(static_cast<QMouseEvent*>(e));
    return QObject::eventFilter(o, e);
}

void MainWindow::loadSettings()
{
    Settings settings;

    settings.window.state.restore(this);
    settings.window.geometry.restore(this);
    settings.window.centralSplitter.state.restore(ui->centralSplitter);
    settings.window.treeSplitter.state.restore(ui->treeSplitter);
    settings.window.mapSplitter.state.restore(ui->mapSplitter);
    settings.window.header.state.restore(ui->tree->header());

    using QSP = QStandardPaths;

    setHistory(settings.dirs.history);
    ui->root->setCurrentText(settings.dirs.root(QSP::writableLocation(QSP::PicturesLocation)));
    ui->filter->setText(settings.filter("*.jpg;*.jpeg"));
}

void MainWindow::saveSettings()
{
    Settings settings;

    settings.window.state.save(this);
    settings.window.geometry.save(this);
    settings.window.centralSplitter.state.save(ui->centralSplitter);
    settings.window.treeSplitter.state.save(ui->treeSplitter);
    settings.window.mapSplitter.state.save(ui->mapSplitter);
    settings.window.header.state.save(ui->tree->header());

    settings.dirs.history = history();
    settings.dirs.root = ui->root->currentText();
    settings.filter = ui->filter->text();

    if (auto dialog = coordEditDialog(CreateOption::Never))
    {
        settings.coordEditDialog.geometry.save(dialog);
        settings.coordEditDialog.header.state.save(dialog->view()->header());
    }

    if (auto dialog = keywordsDialog(CreateOption::Never))
    {
        settings.keywordDialog.geometry.save(dialog);
        settings.keywordDialog.orLogic = dialog->button(KeywordsDialog::Button::Or)->isChecked();
    }
}

void MainWindow::showMapTooltip(const QPoint& pos)
{
    int row = mMapSelectionModel->howeredRow();

    QModelIndex index = mMapModel->index(row, 0);
    QStringList files = mMapModel->data(index, MapPhotoListModel::Role::Files).toStringList();
    if (files.isEmpty())
        return;

    if (files.size() == 1)
    {
        QToolTip::showText(pos, files.first(), this);
        return;
    }

    static GridToolTip* widget = nullptr;
    if (!widget)
    {
        widget = new GridToolTip(this);
        connect(widget->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::syncSelection);
        connect(widget->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::syncCurrentIndex);
        connect(widget, &GridToolTip::doubleClicked, this, [this](const QModelIndex& index){
            if (auto photo = ExifStorage::data(IFileListModel::path(index))) {
                mMapModel->setZoom(18);
                mMapModel->setCenter(photo->position);
            }
        });
    }

    widget->setFiles(files);
    widget->showAt(pos);
    widget->setFocus();
}

void MainWindow::showTooltip(const QPoint& pos, QAbstractItemView* view)
{
    QModelIndex index = view->indexAt(view->viewport()->mapFromGlobal(pos)).siblingAtColumn(0);
    if (!index.isValid() || mTreeModel->isDir(index)) return;
    QString path = mTreeModel->filePath(index);

    static auto widget = new LabelTooltip(this);

    Exif::File exif(path);
    widget->setPixmap(exif.thumbnail(300, 200));
    widget->showAt(pos, 2);
}

void MainWindow::mapClick(const QMouseEvent* e)
{
    if (!ui->actionEditCoords->isChecked())
        return;

    if (mMapSelectionModel->howeredRow() != -1)
        mMapCursor.setCursor(e->type() == QEvent::MouseButtonPress ? Qt::ClosedHandCursor : Qt::OpenHandCursor);

    if (e->type() == QEvent::MouseButtonRelease && e->button() == Qt::LeftButton)
    {
        if (auto map = ui->map->rootObject()->findChild<QObject*>("map"))
        {
            QGeoCoordinate coord;
            QMetaObject::invokeMethod(map, "toCoordinate", Q_RETURN_ARG(QGeoCoordinate, coord), Q_ARG(QPointF, QPointF(e->pos())));
            qDebug() << "QML function returned:" << coord << "HR" << mMapSelectionModel->howeredRow();

            QString path = IFileListModel::path(mMapModel->index(mMapSelectionModel->howeredRow()));
            if (path.isEmpty())
                path = IFileListModel::path(currentView()->currentIndex());
            if (path.isEmpty())
                return;

            coordEditDialog()->setCoords(path, QPointF(coord.latitude(), coord.longitude()));
        }
    }
}

bool MainWindow::mapMouseMove(const QMouseEvent* e)
{
    const bool isEditMode = ui->actionEditCoords->isChecked();
    const bool isOnPicture = mMapSelectionModel->howeredRow() != -1;
    const bool isPressed = (e->buttons() & Qt::LeftButton);

    if (!isEditMode)
        mMapCursor.setCursor(Qt::ArrowCursor);
    else if (!isOnPicture)
        mMapCursor.setCursor(Qt::CrossCursor);
    else {
        mMapCursor.setCursor(isPressed ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    }

    return false;
}

QStringList MainWindow::history() const
{
    QStringList hist;
    for (int i = 0; i < ui->root->count(); ++i)
        if (!hist.contains(ui->root->itemText(i), Qt::CaseInsensitive)) // TODO QtCompat::CaseInsensitive
            hist.append(ui->root->itemText(i));
    return hist;
}

void MainWindow::setHistory(const QStringList& history)
{
    QSignalBlocker lock(ui->root);

    QString text = ui->root->currentText();
    ui->root->clear();
    ui->root->addItems(history);
    ui->root->setCurrentText(text);
}

CoordEditDialog* MainWindow::coordEditDialog(CreateOption createOption)
{
    static CoordEditDialog* dialog = nullptr;
    if (dialog || createOption == CreateOption::Never)
        return dialog;

    Settings settings;

    dialog = new CoordEditDialog(this);
    dialog->view()->setItemDelegateForColumn(CoordEditModel::COLUMN_POSITION, new GeoCoordinateDelegate(dialog)); // TODO incapsulate
    connect(dialog, &CoordEditDialog::apply, this, &MainWindow::saveCoords);
    connect(dialog, &CoordEditDialog::revert, this, &MainWindow::revertCoords);

    settings.coordEditDialog.geometry.restore(dialog);
    settings.coordEditDialog.header.state.restore(dialog->view()->header());

    return dialog;
}

void MainWindow::saveCoords()
{
    Settings settings;

    if (!settings.keywordDialog.overwriteSilently)
    {
        using QMBox = QMessageBox;
        QMBox box(QMBox::Question, "", tr("Overwrite %1 file(s)?").arg(coordEditDialog()->model()->rowCount()), QMBox::Yes | QMBox::No, this);
        box.setCheckBox(new QCheckBox(tr("Do not ask me next time")));
        int ansver = box.exec();
        settings.keywordDialog.overwriteSilently = box.checkBox()->isChecked();
        if (ansver != QMBox::Yes)
            return;
    }

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList warnings;

    for (const QString& path: coordEditDialog()->model()->updated())
    {
        Exif::File file;
        if (!file.load(path))
        {
            warnings.append(tr("Load '%1' failed: %2").arg(path, file.errorString()));
            continue;
        }

        if (auto photo = ExifStorage::data(path))
        {
            double lat = photo->position.x();
            double lon = photo->position.y();

            file.setValue(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE, Exif::Utils::toDMS(std::abs(lat)));
            file.setValue(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE, Exif::Utils::toDMS(std::abs(lon)));
            file.setValue(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE_REF, Exif::Utils::toLatitudeRef(lat));
            file.setValue(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE_REF, Exif::Utils::toLongitudeRef(lon));
        }
        else
        {
            warnings.append(tr("Unable to save '%1': internal application error").arg(path));
        }

        if (file.save(path))
        {
            coordEditDialog()->model()->remove(path);
        }
        else
        {
            warnings.append(tr("Save '%1' failed: %2").arg(path, file.errorString()));
            continue;
        }
    }

    QGuiApplication::restoreOverrideCursor();

    if (warnings.isEmpty())
    {
        coordEditDialog()->button(CoordEditDialog::Button::Apply)->setEnabled(false);
        coordEditDialog()->button(CoordEditDialog::Button::Revert)->setEnabled(false);
    }
    else
        QMessageBox::warning(this, "", warnings.join("\n"));
}

void MainWindow::revertCoords()
{
    auto backup = coordEditDialog()->model()->backedUp();
    for (auto i = backup.cbegin(); i != backup.cend(); ++i)
    {
        const QString& path = i.key();
        const QPointF& pos = i.value();

        if (auto photo = ExifStorage::data(path))
        {
            photo->position = pos;
            emit ExifStorage::instance()->ready(photo);
        }
        else
        {
            qWarning() << "revert" << path << "failed";
        }
    }

    coordEditDialog()->model()->clear();
    coordEditDialog()->button(CoordEditDialog::Button::Apply)->setEnabled(false);
}

KeywordsDialog* MainWindow::keywordsDialog(CreateOption createOption)
{
    static KeywordsDialog* dialog = nullptr;
    if (dialog || createOption == CreateOption::Never)
        return dialog;

    Settings settings;

    dialog = new KeywordsDialog(this);
    settings.keywordDialog.geometry.restore(dialog);
    dialog->button(KeywordsDialog::Button::Or)->setChecked(settings.keywordDialog.orLogic);

    connect(ExifStorage::instance(), &ExifStorage::keywordAdded, this, [this](const QString& keyword, int count){
        dialog->model()->insert(keyword, count); });
    dialog->model()->clear();
    for (const QString& keyword: ExifStorage::keywords())
        dialog->model()->insert(keyword, ExifStorage::count(keyword));
    dialog->view()->resizeColumnToContents(KeywordsModel::COLUMN_KEYWORD); // QHeaderView::ResizeMode doesn't seem to work
    dialog->view()->resizeColumnToContents(KeywordsModel::COLUMN_KEYWORD_COUNT); // TODO incapsulate this

    connect(dialog, &KeywordsDialog::changed, this, &MainWindow::keywordsChanged);
    connect(dialog, &KeywordsDialog::apply, this, &MainWindow::saveKeywords);

    if (currentView()->selectionModel()->hasSelection())
        updateKeywordsDialog(mTreeModel->path(currentSelection()));


    return dialog;
}

void MainWindow::keywordsChanged()
{
    if (keywordsDialog()->mode() == KeywordsDialog::Mode::Filter)
    {
        QStringList keywords = keywordsDialog()->model()->values(Qt::Checked); // TODO encapsulate
        auto logic = keywordsDialog()->button(KeywordsDialog::Button::Or)->isChecked() ? ExifStorage::Logic::Or : ExifStorage::Logic::And;
        auto files = ExifStorage::byKeywords(keywords, logic);
        auto checked = QtCompat::toSet(mCheckedModel->stringList());

        auto toCheck = files - checked;
        auto toUncheck = checked - files;

        for (const auto& path: toCheck)
            mTreeModel->setData(mTreeModel->index(path), Qt::Checked, Qt::CheckStateRole);

        for (const auto& path: toUncheck)
            mTreeModel->setData(mTreeModel->index(path), Qt::Unchecked, Qt::CheckStateRole);
    }
}

void MainWindow::updateKeywordsDialog(const QStringList& selectedFiles)
{
    if (auto dialog = keywordsDialog(CreateOption::Never)) {
        if (dialog->mode() == KeywordsDialog::Mode::Edit) {

            QSet<QString> all, common, partially;

            for (const QString& path: selectedFiles) {
                if (QFileInfo(path).isDir()) continue;
                QString keywordsTag;
                if (auto photo = ExifStorage::data(path))
                    keywordsTag = photo->keywords;
                else
                    keywordsTag = Exif::File(path, false).value(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS).toString();

                QSet<QString> keywords;

                for (QString& s: keywordsTag.split(';'))
                    keywords.insert(s.trimmed());

                if (all.isEmpty()) {
                    all = common = keywords;
                } else {
                    all.unite(keywords);
                    common.intersect(keywords);
                    partially = all - common;
                }
            }

            dialog->model()->setChecked(common, partially);
            dialog->button(KeywordsDialog::Button::Apply)->setEnabled(false);
        }
    }
}

void MainWindow::saveKeywords()
{
    Settings settings;

    if (!currentView()->selectionModel()->hasSelection())
        return;

    QStringList selectedFiles = mTreeModel->path(currentSelection());

    if (!settings.keywordDialog.overwriteSilently) {
        using QMBox = QMessageBox;
        QMBox box(QMBox::Question, "", tr("Overwrite %n file(s)?", nullptr, selectedFiles.size()), QMBox::Yes | QMBox::No, this);
        box.setCheckBox(new QCheckBox(tr("Do not ask me next time")));
        int ansver = box.exec();
        settings.keywordDialog.overwriteSilently = box.checkBox()->isChecked();
        if (ansver != QMBox::Yes)
            return;
    }

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList warnings;

    for (const auto& path: selectedFiles) {
        if (QFileInfo(path).isDir()) continue;
        Exif::File file;
        if (!file.load(path)) {
            warnings.append(tr("Load '%1' failed: %2").arg(path, file.errorString()));
            continue;
        }

        file.setValue(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS, keywordsDialog()->model()->values(Qt::Checked).join(';'));

        if (!file.save(path)) {
            warnings.append(tr("Save '%1' failed: %2").arg(path, file.errorString()));
            continue;
        }

        ExifStorage::parse(path);
    }

    QGuiApplication::restoreOverrideCursor();

    if (warnings.isEmpty()) {
        keywordsDialog()->button(KeywordsDialog::Button::Apply)->setEnabled(false);
        keywordsDialog()->model()->setExtraFlags(Qt::NoItemFlags); // reset
    } else {
        QMessageBox::warning(this, "", warnings.join("\n"));
    }
}

void MainWindow::updatePicture(const QString& path)
{
    if (path.isEmpty() || QFileInfo(path).isDir())
    {
        ui->picture->setPath("");
        ui->picture->setPixmap({});
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

void MainWindow::syncSelection()
{
    if (auto source = qobject_cast<QItemSelectionModel*>(sender()))
    {
        auto& previousSelection = mSelection[source];
        auto currentSelection = source->model() == mTreeModel ? source->selectedRows() : source->selectedIndexes();
        if (previousSelection != currentSelection)
        {
            previousSelection = currentSelection;

            QStringList selectedFiles = IFileListModel::path(currentSelection);

            // qDebug() << __func__ << "from" << source->objectName() << selectedFiles;

            if (source != ui->tree->selectionModel())
                applySelection(ui->tree, selectedFiles);
            if (source != ui->list->selectionModel())
                applySelection(ui->list, selectedFiles);
            if (source != ui->checked->selectionModel())
                applySelection(ui->checked, selectedFiles);
            if (source != mMapSelectionModel)
                applySelection(mMapSelectionModel, selectedFiles);

            updateKeywordsDialog(selectedFiles);
        }
    }
}

void MainWindow::applySelection(QAbstractItemView* to, const QStringList& selectedFiles)
{
    applySelection(to->selectionModel(), selectedFiles);
}

void MainWindow::applySelection(QItemSelectionModel* to, const QStringList& selectedFiles)
{
    if (!to || !to->model())
    {
        qWarning() << Q_FUNC_INFO << "invalid arguments";
        return;
    }

    if (auto model = dynamic_cast<IFileListModel*>(to->model()))
    {
        QModelIndexList selection;
        for (const QString& path: selectedFiles)
        {
            auto i = model->index(path);
            if (i.isValid())
                selection.append(model->index(path));
        }

        QModelIndexList& previousSelection = mSelection[to];
        if (previousSelection != selection)
        {
            // qDebug() << __func__ << "to" << to->objectName() << selectedFiles;

            using QSM = QItemSelectionModel;

            previousSelection = selection;

            QItemSelection range;
            for (const auto& i : selection)
                range.select(i, i);
            to->select(range, QSM::Clear | QSM::Select | QSM::Rows);
        }
    }
}

void MainWindow::syncCurrentIndex(const QModelIndex& currentIndex)
{
    if (auto source = qobject_cast<QItemSelectionModel*>(sender()))
    {
        QModelIndex& previousIndex = mCurrentIndex[source];
        if (previousIndex != currentIndex)
        {

            previousIndex = currentIndex;
            QString path = IFileListModel::path(currentIndex);

            // qDebug() << __func__ << "from" << source->objectName() << path;

            if (source != ui->tree->selectionModel())
                applyCurrentIndex(ui->tree, path);
            if (source != ui->list->selectionModel())
                applyCurrentIndex(ui->list, path);
            if (source != ui->checked->selectionModel())
                applyCurrentIndex(ui->checked, path);
            if (source != mMapSelectionModel)
                applyCurrentIndex(mMapSelectionModel, path);

            updatePicture(path);
        }

    }
}

void MainWindow::applyCurrentIndex(QAbstractItemView* to, const QString& path)
{
    applyCurrentIndex(to->selectionModel(), path, to);
}

void MainWindow::applyCurrentIndex(QItemSelectionModel* to, const QString& path, QAbstractItemView* view)
{
    if (!to || !to->model())
    {
        qWarning() << Q_FUNC_INFO << "invalid arguments";
        return;
    }

    if (auto model = dynamic_cast<IFileListModel*>(to->model()))
    {
        QModelIndex& previous = mCurrentIndex[to];
        QModelIndex current = model->index(path);
        if (previous != current)
        {
            using QSM = QItemSelectionModel;

            // qDebug() << __func__ << "to" << to->objectName() << path;

            previous = current;

            if (view == ui->list)
                ui->list->setRootIndex(current.parent());

            to->setCurrentIndex(current, QSM::Current);

            if (view && view->isVisible())
                view->scrollTo(current);
        }
    }
}

QAbstractItemView *MainWindow::currentView() const
{
    return ui->actionIconView->isChecked() ?
               static_cast<QAbstractItemView*>(ui->list) :
               static_cast<QAbstractItemView*>(ui->tree);
}

QModelIndexList MainWindow::currentSelection() const
{
    return ui->actionIconView->isChecked() ?
               ui->list->selectionModel()->selectedIndexes() :
               ui->tree->selectionModel()->selectedRows();
}

void MainWindow::on_pickRoot_clicked()
{
    QString root = ui->root->currentText();
    root = QFileDialog::getExistingDirectory(this, tr("Select root path"), root);
    if (root.isEmpty())
        return;

    ui->root->setCurrentText(root);
}

void MainWindow::on_keywords_clicked()
{
    keywordsDialog()->show();
}

static QStringList uconcat(const QString& text, QStringList list) {
    list.removeAll(text);
    list.prepend(text);
    return list;
}

void MainWindow::on_root_currentTextChanged(const QString& text)
{
    QFileInfo dir(text);
    if (!dir.isDir() || !dir.exists())
        return;

    mTreeModel->setRootPath(text);
    auto root = mTreeModel->index(text);
    ui->tree->setRootIndex(root);
    ui->list->setRootIndex(root);
    mMapModel->clear();
    setHistory(uconcat(text, history()));
}

void MainWindow::on_filter_textChanged(const QString& text)
{
    mTreeModel->setNameFilters(text.split(";"));
    mTreeModel->setNameFilterDisables(false);
}

void MainWindow::on_tree_doubleClicked(const QModelIndex& index)
{
    if (ui->stackedWidget->currentWidget() == ui->pageList)
    {
        if (mTreeModel->isDir(index))
        {
            QDir dir;
            if (index.data() == "..")
            {
                dir.setPath(mTreeModel->filePath(ui->list->rootIndex()));
                dir.cdUp();
            }
            else
            {
                dir.setPath(mTreeModel->filePath(index));
            }
            ui->list->setRootIndex(mTreeModel->index(dir.absolutePath()));
        }
    }

    auto coords = index.siblingAtColumn(FileTreeModel::COLUMN_COORDS).data().toPointF();
    if (!coords.isNull())
    {
        mMapModel->setZoom(18);
        mMapModel->setCenter(coords);
    }
}

void MainWindow::on_actionCheck_triggered()
{
    for (const auto& tid: currentSelection())
        mTreeModel->setData(tid, Qt::Checked, Qt::CheckStateRole);
}


void MainWindow::on_actionUncheck_triggered()
{
    for (const auto& tid: currentSelection())
        mTreeModel->setData(tid, Qt::Unchecked, Qt::CheckStateRole);
}

void MainWindow::on_actionUncheckAll_triggered()
{
    for (const auto& tid: Checker::children(mTreeModel, Qt::Checked))
        mTreeModel->setData(tid, Qt::Unchecked, Qt::CheckStateRole);
}

void MainWindow::on_actionEditKeywords_triggered(bool checked)
{
    keywordsDialog()->setMode(checked ? KeywordsDialog::Mode::Edit : KeywordsDialog::Mode::Filter);

    if (keywordsDialog()->mode() == KeywordsDialog::Mode::Filter)
        keywordsDialog()->model()->setChecked({}, {});
    else
        updateKeywordsDialog(mTreeModel->path(currentSelection()));

    ui->actionCopyKeywords->setVisible(checked);
    ui->actionPasteKeywords->setVisible(checked);

    if (checked)
        keywordsDialog()->show();
}

void MainWindow::on_actionEditCoords_triggered(bool checked)
{
    mMapCursor.setCursor(checked ? Qt::CrossCursor : Qt::ArrowCursor);
    if (checked)
    {
        for (const auto& i: currentSelection())
            mTreeModel->setData(i, Qt::Checked, Qt::CheckStateRole);
        coordEditDialog()->show();
    }

    ui->actionCopyCoords->setVisible(checked);
    ui->actionPasteCoords->setVisible(checked);
}

void MainWindow::on_actionIconView_toggled(bool toggled)
{
    ui->stackedWidget->setCurrentWidget(toggled ? ui->pageList : ui->pageTree);
    mTreeModel->setFilter(toggled ?
                            mTreeModel->filter() & ~QDir::NoDotDot :
                            mTreeModel->filter() | QDir::NoDotDot);
}

void MainWindow::on_actionCopyKeywords_triggered()
{
    QString path = IFileListModel::path(currentView()->currentIndex());
    if (auto photo = ExifStorage::data(path))
    {
        QGuiApplication::clipboard()->setText(photo->keywords);
    }
}


void MainWindow::on_actionPasteKeywords_triggered()
{
    if (auto dialog = keywordsDialog(CreateOption::Never))
    {
        QString clipboardText = QGuiApplication::clipboard()->text();
        if (clipboardText.isEmpty()) return;
        QStringList existingKeywords = dialog->model()->values();
        QStringList clipboardKeywords = clipboardText.split(';');
        for (const QString& kw: clipboardKeywords)
        {
            if (!existingKeywords.contains(kw))
            {
                int resp = QMessageBox::question(dialog, tr("Paste keywords"), tr("Paste '%1'?").arg(clipboardText));
                if (resp != QMessageBox::Yes)
                    return;
                break;
            }
        }

        for (const QString& kw: clipboardKeywords)
        {
            int row = existingKeywords.indexOf(kw);
            QModelIndex index = row == -1 ?
                dialog->model()->insert(kw) :
                dialog->model()->index(row);
            dialog->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
        }
    }
}

void MainWindow::on_actionCopyCoords_triggered()
{
    QString path = IFileListModel::path(currentView()->currentIndex());
    if (auto photo = ExifStorage::data(path))
    {
        if (!photo->position.isNull())
        {
            QGeoCoordinate coord(photo->position.x(), photo->position.y());
            QGuiApplication::clipboard()->setText(coord.toString(QGeoCoordinate::DegreesWithHemisphere));
        }
    }
}

void MainWindow::on_actionPasteCoords_triggered()
{
    QGeoCoordinate coord = GeoCoordinate::fromString(QGuiApplication::clipboard()->text());
    if (coord.isValid())
    {
        if (auto dialog = coordEditDialog(CreateOption::Never))
        {
            QString path = IFileListModel::path(currentView()->currentIndex());
            dialog->setCoords(path, QPointF(coord.latitude(), coord.longitude()));
        }
    }
}

void MainWindow::on_actionProxySave_triggered()
{
    auto coordDialog = coordEditDialog(CreateOption::Never);
    auto kwordDialog = keywordsDialog(CreateOption::Never);
    bool coord = coordDialog && coordDialog->isVisible();
    bool kword = kwordDialog && kwordDialog->isVisible();
    if (coord && kword)
        return;

    if (coord)
        if (auto button = coordDialog->button(CoordEditDialog::Button::Apply))
            if (button->isEnabled())
                button->click();

    if (kword)
        if (auto button = kwordDialog->button(KeywordsDialog::Button::Apply))
            if (button->isEnabled())
                button->click();
}
