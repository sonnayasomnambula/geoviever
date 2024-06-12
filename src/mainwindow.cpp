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
#include <QScreen>
#include <QScrollBar>
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
        struct { State state = "window/verticalSplitter.state"; } verticalSplitter;
        struct { State state = "window/horizontalSplitter.state"; } horizontalSplitter;
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
    , mMapModel(new MapPhotoListModel)
{
    ui->setupUi(this);
    ui->tree->addActions({ ui->actionCheck, ui->actionUncheck, ui->actionEditKeywords });

    auto comboDelegate = new ItemButtonDelegate(QImage(":/cross-small.png"), ui->root);
    ui->root->setItemDelegate(comboDelegate);
    connect(comboDelegate, &ItemButtonDelegate::buttonPressed, ui->root, &QComboBox::removeItem);

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

    connect(mTreeModel, &FileTreeModel::fileRenamed, this, [this](const QString& path, const QString& oldName, const QString& newName){
        // TODO
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
    settings.window.horizontalSplitter.state.save(ui->horizontalSplitter);
    settings.window.verticalSplitter.state.save(ui->verticalSplitter);
    settings.window.header.state.save(ui->tree->header());

    settings.dirs.history = history();
    settings.dirs.root = ui->root->currentText();
    settings.filter = ui->filter->text();

    if (auto dialog = keywordsDialog(CreateOption::Never))
        settings.keywordDialog.geometry.save(dialog);
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
            dialog->model()->add(keyword, count); });
    dialog->model()->clear();
    for (const QString& keyword: ExifStorage::keywords())
        dialog->model()->add(keyword, ExifStorage::count(keyword));
    dialog->view()->resizeColumnToContents(KeywordsModel::COLUMN_KEYWORD); // QHeaderView::ResizeMode doesn't seem to work
    dialog->view()->resizeColumnToContents(KeywordsModel::COLUMN_KEYWORD_COUNT); // TODO incapsulate this

    connect(dialog, &KeywordsDialog::checkChanged, this, &MainWindow::keywordChecked);
    connect(ui->tree->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateKeywordsDialog);
    connect(dialog, &KeywordsDialog::apply, this, &MainWindow::saveKeywords);

    if (ui->tree->selectionModel()->hasSelection())
        updateKeywordsDialog();

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

void MainWindow::updateKeywordsDialog()
{
    if (keywordsDialog()->mode() == KeywordsDialog::Mode::Edit) {

        QSet<QString> common, partially;

        for (const auto& index: ui->tree->selectionModel()->selectedRows()) {
            if (mTreeModel->isDir(index)) continue;
            QString path = mTreeModel->filePath(index);
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

        keywordsDialog()->model()->setChecked(common, partially);
        keywordsDialog()->button(KeywordsDialog::Button::Apply)->setEnabled(false);
    }
}

void MainWindow::saveKeywords()
{
    Settings settings;

    auto selection = ui->tree->selectionModel()->selectedRows();
    if (selection.isEmpty())
        return;

    if (!settings.keywordDialog.overwriteSilently) {
        using QMBox = QMessageBox;
        QMBox box(QMBox::Question, "", tr("Overwrite %1 file(s)?").arg(selection.size()), QMBox::Yes | QMBox::No, this);
        box.setCheckBox(new QCheckBox(tr("Do not ask me next time")));
        int ansver = box.exec();
        settings.keywordDialog.overwriteSilently = box.checkBox()->isChecked();
        if (ansver != QMBox::Yes)
            return;
    }

    for (const auto& index: selection) {
        if (mTreeModel->isDir(index)) continue;
        QString path = mTreeModel->filePath(index);
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

    keywordsDialog()->button(KeywordsDialog::Button::Apply)->setEnabled(false);
    keywordsDialog()->model()->setExtraFlags(Qt::NoItemFlags); // reset
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
    ui->tree->setRootIndex(mTreeModel->index(text));
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
    auto coords = index.siblingAtColumn(FileTreeModel::COLUMN_COORDS).data().toPointF();
    if (!coords.isNull())
    {
        mMapModel->setZoom(18);
        mMapModel->setCenter(coords);
    }
}

void MainWindow::on_actionCheck_triggered()
{
    for (const auto& tid: ui->tree->selectionModel()->selectedRows())
        mTreeModel->setData(tid, Qt::Checked, Qt::CheckStateRole);
}


void MainWindow::on_actionUncheck_triggered()
{
    for (const auto& tid: ui->tree->selectionModel()->selectedRows())
        mTreeModel->setData(tid, Qt::Unchecked, Qt::CheckStateRole);
}

void MainWindow::on_actionEditKeywords_triggered(bool checked)
{
    keywordsDialog()->setMode(checked ? KeywordsDialog::Mode::Edit : KeywordsDialog::Mode::Filter);

    if (keywordsDialog()->mode() == KeywordsDialog::Mode::Filter)
        keywordsDialog()->model()->setChecked({}, {});
    else
        updateKeywordsDialog();

    keywordsDialog()->show();
}

