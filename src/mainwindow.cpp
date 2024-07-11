#include <QAbstractTableModel>
#include <QCheckBox>
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
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QToolTip>

#include <cmath>

#include "exif/file.h"

#include "abstractsettings.h"
#include "exifstorage.h"
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
    } keywordDialog;
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

    ui->actionSeparator1->setSeparator(true);
    ui->actionSeparator2->setSeparator(true);

    auto viewGroup = new QActionGroup(this);
    viewGroup->addAction(ui->actionIconView);
    viewGroup->addAction(ui->actionTreeView);

    QList<QAction*> actions = { ui->actionCheck, ui->actionUncheck,
                                ui->actionSeparator1,
                                ui->actionEditKeywords,
                                ui->actionSeparator2,
                                ui->actionIconView, ui->actionTreeView };
    ui->tree->addActions(actions);
    ui->list->addActions(actions);

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
        if (checked) {
            mMapModel->insert(path);
            mCheckedModel->insert(path);
        } else {
            mMapModel->remove(path);
            mCheckedModel->remove(path);
        }
    });

    connect(ExifStorage::instance(), &ExifStorage::ready, mMapModel, &MapPhotoListModel::update);
    connect(ExifStorage::instance(), &ExifStorage::remains, this, [this](int count){
        static QElapsedTimer timer;
        if (count && timer.isValid() && timer.elapsed() < 500)
            return;
        statusBar()->showMessage(count ? QString::number(count) : tr("Ready"), count ? 500 : 5000);
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

    if (auto dialog = keywordsDialog(CreateOption::Never))
        settings.keywordDialog.geometry.save(dialog);
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

KeywordsDialog* MainWindow::keywordsDialog(CreateOption createOption)
{
    static KeywordsDialog* dialog = nullptr;
    if (dialog || createOption == CreateOption::Never)
        return dialog;

    Settings settings;

    dialog = new KeywordsDialog(this);
    settings.keywordDialog.geometry.restore(dialog);

    connect(ExifStorage::instance(), &ExifStorage::keywordAdded, this, [this](const QString& keyword, int count){
        dialog->model()->insert(keyword, count); });
    dialog->model()->clear();
    for (const QString& keyword: ExifStorage::keywords())
        dialog->model()->insert(keyword, ExifStorage::count(keyword));
    dialog->view()->resizeColumnToContents(KeywordsModel::COLUMN_KEYWORD); // QHeaderView::ResizeMode doesn't seem to work
    dialog->view()->resizeColumnToContents(KeywordsModel::COLUMN_KEYWORD_COUNT); // TODO incapsulate this

    connect(dialog, &KeywordsDialog::checkChanged, this, &MainWindow::keywordChecked);
    connect(dialog, &KeywordsDialog::apply, this, &MainWindow::saveKeywords);

    if (currentView()->selectionModel()->hasSelection())
        updateKeywordsDialog(mTreeModel->path(currentSelection()));


    return dialog;
}

void MainWindow::keywordChecked(const QString& keyword, Qt::CheckState)
{
    if (keywordsDialog()->mode() == KeywordsDialog::Mode::Filter) {
        QSet<QString> keywords = QtCompat::toSet(keywordsDialog()->model()->values(Qt::Checked));
        QMap<QString, QModelIndex> indexes;

        // 1. uncheck checked
        for (const QModelIndex& tid: Checker::children(mTreeModel, Qt::Checked, ui->tree->rootIndex()))
            indexes[mTreeModel->filePath(tid)] = tid;

        // 2. check unchecked
        for (const QString& file: ExifStorage::byKeyword(keyword)) {
            QModelIndex tid = mTreeModel->index(file);
            if (tid.isValid()) {
                indexes[file] = tid;
            }
        }

        for (auto i = indexes.cbegin(); i != indexes.cend(); ++i) {
            const auto& file = i.key();
            const auto& tid = i.value();
            Qt::CheckState state = keywords.intersects(QtCompat::toSet(ExifStorage::keywords(file))) ? Qt::Checked : Qt::Unchecked;
            mTreeModel->setData(tid, state, Qt::CheckStateRole);
        }
    }
}

void MainWindow::updateKeywordsDialog(const QStringList& selectedFiles)
{
    if (auto dialog = keywordsDialog(CreateOption::Never)) {
        if (dialog->mode() == KeywordsDialog::Mode::Edit) {

            QSet<QString> common, partially;

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

                if (common.isEmpty() && partially.isEmpty()) {
                    common = keywords;
                } else {
                    common.intersect(keywords);
                    partially = (common | partially | keywords) - common;
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
        QMBox box(QMBox::Question, "", tr("Overwrite %1 file(s)?").arg(selectedFiles.size()), QMBox::Yes | QMBox::No, this);
        box.setCheckBox(new QCheckBox(tr("Do not ask me next time")));
        int ansver = box.exec();
        settings.keywordDialog.overwriteSilently = box.checkBox()->isChecked();
        if (ansver != QMBox::Yes)
            return;
    }

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    for (const auto& path: selectedFiles) {
        if (QFileInfo(path).isDir()) continue;
        Exif::File file;
        if (!file.load(path)) {
            QMessageBox::warning(this, "", tr("Load '%1' failed: %2").arg(path, file.errorString()));
            return;
        }

        file.setValue(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS, keywordsDialog()->model()->values(Qt::Checked).join(';'));

        if (!file.save(path)) {
            QMessageBox::warning(this, "", tr("Save '%1' failed: %2").arg(path, file.errorString()));
            return;
        }

        ExifStorage::instance()->parse(path);
    }

    QGuiApplication::restoreOverrideCursor();

    keywordsDialog()->button(KeywordsDialog::Button::Apply)->setEnabled(false);
    keywordsDialog()->model()->setExtraFlags(Qt::NoItemFlags); // reset
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
            to->setCurrentIndex(current, QSM::Current);

            if (view && view->isVisible())
                view->scrollTo(to->currentIndex());
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

void MainWindow::on_actionEditKeywords_triggered(bool checked)
{
    keywordsDialog()->setMode(checked ? KeywordsDialog::Mode::Edit : KeywordsDialog::Mode::Filter);

    if (keywordsDialog()->mode() == KeywordsDialog::Mode::Filter)
        keywordsDialog()->model()->setChecked({}, {});
    else
        updateKeywordsDialog(mTreeModel->path(currentSelection()));

    keywordsDialog()->show();
}

void MainWindow::on_actionIconView_toggled(bool toggled)
{
    ui->stackedWidget->setCurrentWidget(toggled ? ui->pageList : ui->pageTree);
    mTreeModel->setFilter(toggled ?
                            mTreeModel->filter() & ~QDir::NoDotDot :
                            mTreeModel->filter() | QDir::NoDotDot);
}
