#include "mainwindow.h"

#include "carddelegate.h"
#include "detailspanel.h"
#include "groupsdialog.h"
#include "guiutil.h"
#include "launcher.h"
#include "paths.h"
#include "pathutil.h"
#include "progress.h"
#include "quickopen.h"
#include "settingsdialog.h"
#include "sidebar.h"
#include "tagdialog.h"
#include "thingyimport.h"
#include "thumbnailloader.h"

#include <sheaf/version.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QStyleOptionViewItem>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QShortcut>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent>

#include <functional>

// Resources of a static library need to be asked for by name, and this has to happen outside any namespace
static void initResources()
{
    Q_INIT_RESOURCE(resources);
}

namespace sheaf {
namespace gui {

namespace {

QIcon icon(const char *name, QStyle::StandardPixmap fallback = QStyle::SP_CustomBase)
{
    const QIcon themed = QIcon::fromTheme(QString::fromLatin1(name));
    if (!themed.isNull() || fallback == QStyle::SP_CustomBase) {
        return themed;
    }
    return QApplication::style()->standardIcon(fallback);
}

Group everythingGroup()
{
    Group group;
    group.id = QStringLiteral("all");
    group.patterns = {QStringLiteral("*")};
    return group;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    initResources();
    setWindowTitle(tr("Sheaf"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("sheaf"), QIcon(QStringLiteral(":/icons/sheaf.png"))));
    setAcceptDrops(true);
    setMinimumSize(560, 380);

    QString error;
    if (!m_db.open(paths::databaseFile(), &error)) {
        QMessageBox::critical(this, tr("Sheaf"), tr("The library database could not be opened.\n\n%1").arg(error));
        return;
    }

    m_settings = Settings::load(paths::settingsFile());
    QString groupsWarning;
    m_groups = GroupsConfig::load(paths::groupsFile(), &groupsWarning);
    m_activeGroup = m_groups.activeGroup();

    m_loader = new ThumbnailLoader(this);
    m_loader->setGenerateEnabled(m_settings.generateThumbnails);
    m_model = new DocumentModel(this);
    m_proxy = new DocumentFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortOptions(m_settings.sort);
    m_settings.showMissing = false;
    m_proxy->setHideMissing(true);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(800);
    connect(m_saveTimer, &QTimer::timeout, this, &MainWindow::saveState);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_sidebar = new Sidebar(m_splitter);
    m_sidebar->setMinimumWidth(190);
    m_sidebar->setMaximumWidth(340);
    m_sidebar->setContextMenuPolicy(Qt::CustomContextMenu);

    m_stack = new QStackedWidget(m_splitter);
    m_view = new QListView(m_stack);
    m_delegate = new CardDelegate(m_loader, this);
    m_view->setModel(m_proxy);
    m_view->setItemDelegate(m_delegate);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setUniformItemSizes(true);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setMouseTracking(true);
    m_view->setDragEnabled(true);
    m_view->setDragDropMode(QAbstractItemView::DragOnly);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setLayoutMode(QListView::Batched);
    m_view->setBatchSize(200);
    m_view->setAccessibleName(tr("Files"));
    m_view->installEventFilter(this);
    m_view->viewport()->installEventFilter(this);
    m_stack->addWidget(m_view);

    m_empty = new QWidget(m_stack);
    auto *emptyLayout = new QVBoxLayout(m_empty);
    emptyLayout->addStretch(1);
    auto *emptyIcon = new QLabel(m_empty);
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyIcon->setPixmap(QIcon(QStringLiteral(":/icons/sheaf.png")).pixmap(112, 112));
    m_emptyTitle = new QLabel(m_empty);
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_emptyTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
    m_emptyTitle->setFont(titleFont);
    m_emptyText = new QLabel(m_empty);
    m_emptyText->setAlignment(Qt::AlignCenter);
    m_emptyText->setWordWrap(true);
    m_empty->setAutoFillBackground(true);
    m_empty->setBackgroundRole(QPalette::Base);
    m_empty->installEventFilter(this);
    m_empty->setContextMenuPolicy(Qt::CustomContextMenu);
    m_emptyButton = new QPushButton(tr("Add Files..."), m_empty);
    emptyLayout->addWidget(emptyIcon);
    emptyLayout->addWidget(m_emptyTitle);
    emptyLayout->addWidget(m_emptyText, 0, Qt::AlignHCenter);
    emptyLayout->addSpacing(8);
    emptyLayout->addWidget(m_emptyButton, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(1);
    m_stack->addWidget(m_empty);
    connect(m_emptyButton, &QPushButton::clicked, this, &MainWindow::addFiles);

    m_details = new DetailsPanel(m_loader, m_splitter);
    m_details->setMaximumWidth(380);
    m_splitter->addWidget(m_sidebar);
    m_splitter->addWidget(m_stack);
    m_splitter->addWidget(m_details);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setSizes({220, 700, 300});
    setCentralWidget(m_splitter);
    m_details->setVisible(m_settings.showDetails);

    buildActions();
    buildToolbar();

    m_status = new QLabel(this);
    m_status->setAccessibleName(tr("Status"));
    m_busy = new QLabel(tr("Updating..."), this);
    m_busy->setVisible(false);
    statusBar()->addWidget(m_status, 1);
    statusBar()->addPermanentWidget(m_busy);

    connect(m_sidebar, &Sidebar::entryActivated, this, &MainWindow::onSidebarActivated);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { onSelectionChanged(); });
    connect(m_view, &QListView::activated, this, [this](const QModelIndex &) { openDocuments(selectedDocuments()); });
    connect(m_view, &QListView::customContextMenuRequested, this, &MainWindow::showContextMenu);
    connect(m_loader, &ThumbnailLoader::thumbnailReady, m_view->viewport(), QOverload<>::of(&QWidget::update));
    connect(m_empty, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) { m_appMenu->popup(m_empty->mapToGlobal(pos)); });
    connect(m_sidebar, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) { m_appMenu->popup(m_sidebar->mapToGlobal(pos)); });

    m_quick = new QuickOpen(m_model, m_loader);
    connect(m_quick, &QuickOpen::openRequested, this, [this](const DocumentInfo &doc) { openDocuments({doc}); });
    connect(m_quick, &QuickOpen::dismissed, this, [this]() {
        if (m_quickOnly && !isVisible()) {
            QTimer::singleShot(150, qApp, &QCoreApplication::quit);
        }
    });

    m_index = new DocumentIndex(&m_db, this);
    m_index->setOptions(scanOptions());
    connect(m_index, &DocumentIndex::documentsChanged, this, &MainWindow::onDocumentsChanged);
    connect(m_index, &DocumentIndex::busyChanged, this, [this](bool busy) {
        m_busy->setVisible(busy);
        updateEmptyState();
    });
    connect(m_index, &DocumentIndex::warning, this, [this](const QString &message) { flash(message); });

    auto *searchShortcut = new QShortcut(QKeySequence(Qt::Key_Slash), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        m_search->setFocus();
        m_search->selectAll();
    });

    applyViewMode();
    applySortOptions();
    rebuildSidebar();
    applyFilter();
    updateEmptyState();
    updateActions();
    if (!m_settings.geometry.isEmpty()) {
        restoreGeometry(m_settings.geometry);
    } else {
        resize(1100, 680);
    }
    if (!m_settings.splitterState.isEmpty()) {
        m_splitter->restoreState(m_settings.splitterState);
    }

    if (!m_db.recoveryNote().isEmpty()) {
        flash(m_db.recoveryNote());
    }
    if (!groupsWarning.isEmpty()) {
        flash(groupsWarning);
    }
    m_ready = true;
    QTimer::singleShot(0, m_index, &DocumentIndex::start);
}

MainWindow::~MainWindow()
{
    delete m_quick;
}

ScanOptions MainWindow::scanOptions() const
{
    ScanOptions options;
    options.recentFile = paths::recentFile();
    options.kdeDir = paths::kdeRecentDir();
    options.includeKde = m_settings.includeKde;
    // Missing files are always scanned, the list hides them unless the setting asks for them
    options.showMissing = true;
    options.exclusions = m_settings.exclusions;
    options.maxItems = m_settings.maxItems;
    return options;
}

void MainWindow::buildActions()
{
    auto make = [this](const QString &text, const QKeySequence &shortcut, const QIcon &actionIcon, std::function<void()> slot) {
        auto *action = new QAction(actionIcon, text, this);
        if (!shortcut.isEmpty()) {
            action->setShortcut(shortcut);
            action->setShortcutContext(Qt::WindowShortcut);
        }
        connect(action, &QAction::triggered, this, [slot]() { slot(); });
        addAction(action);
        return action;
    };

    m_actOpen = make(tr("&Open"), QKeySequence(Qt::CTRL | Qt::Key_Return), icon("document-open"), [this]() { openDocuments(selectedDocuments()); });
    m_actFolder = make(tr("Show in &Folder"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), icon("folder-open"), [this]() { showInFolder(); });
    m_actFavorite = make(tr("Add to &Favorites"), QKeySequence(Qt::CTRL | Qt::Key_D), icon("bookmark-new"), [this]() { toggleFavorite(); });
    m_actTags = make(tr("Edit &Tags..."), QKeySequence(Qt::CTRL | Qt::Key_T), icon("tag"), [this]() { editTags(); });
    m_actRemove = make(tr("&Remove from Recent List"), QKeySequence(Qt::Key_Delete), icon("list-remove"), [this]() { removeFromList(); });
    m_actRemove->setToolTip(tr("Takes the file off the list. The file itself is not touched."));
    m_actTrash = make(tr("Move to &Trash"), QKeySequence(Qt::SHIFT | Qt::Key_Delete), icon("user-trash"), [this]() { moveToTrash(); });
    m_actCopyPath = make(tr("Copy &Path"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), icon("edit-copy"), [this]() { copyPath(); });
    m_actCopyFile = make(tr("&Copy File"), QKeySequence::Copy, icon("edit-copy"), [this]() { copyFile(); });
    m_actLocate = make(tr("&Locate File..."), QKeySequence(), icon("edit-find"), [this]() { locateFile(); });

    m_actGrid = new QAction(tr("Grid"), this);
    m_actList = new QAction(tr("List"), this);
    m_actGrid->setCheckable(true);
    m_actList->setCheckable(true);
    m_actGrid->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    m_actList->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    m_actGrid->setToolTip(tr("Show files as a grid of thumbnails"));
    m_actList->setToolTip(tr("Show files as a list"));
    auto *viewGroup = new QActionGroup(this);
    viewGroup->addAction(m_actGrid);
    viewGroup->addAction(m_actList);
    (m_settings.gridView ? m_actGrid : m_actList)->setChecked(true);
    connect(viewGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        m_settings.gridView = action == m_actGrid;
        applyViewMode();
        m_saveTimer->start();
    });
    addAction(m_actGrid);
    addAction(m_actList);

    m_actDetails = new QAction(tr("Details"), this);
    m_actDetails->setCheckable(true);
    m_actDetails->setChecked(m_settings.showDetails);
    m_actDetails->setShortcut(QKeySequence(Qt::Key_F9));
    m_actDetails->setToolTip(tr("Show or hide the details panel"));
    connect(m_actDetails, &QAction::toggled, this, [this](bool on) {
        m_settings.showDetails = on;
        m_details->setVisible(on);
        m_saveTimer->start();
    });
    addAction(m_actDetails);

    m_actRefresh = new QAction(icon("view-refresh"), tr("&Refresh"), this);
    m_actRefresh->setShortcut(QKeySequence::Refresh);
    connect(m_actRefresh, &QAction::triggered, this, [this]() { m_index->refresh(); });
    addAction(m_actRefresh);

    m_actQuick = new QAction(tr("&Quick Open"), this);
    m_actQuick->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(m_actQuick, &QAction::triggered, this, &MainWindow::showQuickOpen);
    addAction(m_actQuick);

    auto *find = new QAction(tr("&Search"), this);
    find->setShortcut(QKeySequence::Find);
    connect(find, &QAction::triggered, this, [this]() {
        m_search->setFocus();
        m_search->selectAll();
    });
    addAction(find);

    // Jump straight to a group with Ctrl and a number
    for (int i = 1; i <= 9; ++i) {
        auto *jump = new QShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_0 + i)), this);
        connect(jump, &QShortcut::activated, this, [this, i]() {
            int seen = 0;
            for (int row = 0; row < m_sidebar->count(); ++row) {
                if (m_sidebar->item(row)->data(Sidebar::KindRole).toInt() == Sidebar::GroupKind && ++seen == i) {
                    m_sidebar->setCurrentRow(row);
                    return;
                }
            }
        });
    }
}

void MainWindow::buildToolbar()
{
    auto *bar = addToolBar(tr("Main"));
    bar->setObjectName(QStringLiteral("mainToolbar"));
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->setContextMenuPolicy(Qt::CustomContextMenu);
    bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connect(bar, &QWidget::customContextMenuRequested, this, [this, bar](const QPoint &pos) { m_appMenu->popup(bar->mapToGlobal(pos)); });

    m_search = new QLineEdit(bar);
    m_search->setPlaceholderText(tr("Search files (try tag:work or type:pdf)"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(280);
    m_search->setMaximumWidth(520);
    m_search->setAccessibleName(tr("Search files"));
    m_search->setAccessibleDescription(tr("Searches the whole library. Operators: tag:, type:, in:, is:fav, is:missing, and a minus sign to exclude."));
    m_searchIcon = m_search->addAction(QIcon(), QLineEdit::LeadingPosition);
    m_search->installEventFilter(this);
    bar->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged, this, [this]() {
        applyFilter();
        updateEmptyState();
        updateStatus();
    });

    auto *spacer = new QWidget(bar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(spacer);

    m_sortButton = new QToolButton(bar);
    m_sortButton->setText(tr("Sort"));
    m_sortButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_sortButton->setToolTip(tr("Change the order of the files"));
    m_sortButton->setPopupMode(QToolButton::InstantPopup);
    m_sortButton->setAccessibleName(tr("Sort order"));
    m_sortMenu = new QMenu(tr("Sort By"), this);
    m_sortGroup = new QActionGroup(this);
    const struct {
        const char *text;
        SortMode mode;
    } modes[] = {
        {QT_TR_NOOP("Last Used"), SortMode::LastUsed}, {QT_TR_NOOP("Name"), SortMode::Name}, {QT_TR_NOOP("Date Modified"), SortMode::Modified},
        {QT_TR_NOOP("Size"), SortMode::Size},          {QT_TR_NOOP("Type"), SortMode::Type},
    };
    for (const auto &entry : modes) {
        QAction *action = m_sortMenu->addAction(tr(entry.text));
        action->setCheckable(true);
        action->setData(static_cast<int>(entry.mode));
        m_sortGroup->addAction(action);
        if (entry.mode == m_settings.sort.mode) {
            action->setChecked(true);
        }
    }
    m_sortMenu->addSeparator();
    m_actReverse = m_sortMenu->addAction(tr("Reverse Order"));
    m_actReverse->setCheckable(true);
    m_actReverse->setChecked(m_settings.sort.reversed);
    m_actFavoritesFirst = m_sortMenu->addAction(tr("Favorites First"));
    m_actFavoritesFirst->setCheckable(true);
    m_actFavoritesFirst->setChecked(m_settings.sort.favoritesFirst);
    m_sortButton->setMenu(m_sortMenu);
    bar->addWidget(m_sortButton);
    connect(m_sortGroup, &QActionGroup::triggered, this, [this]() { applySortOptions(); });
    connect(m_actReverse, &QAction::toggled, this, [this]() { applySortOptions(); });
    connect(m_actFavoritesFirst, &QAction::toggled, this, [this]() { applySortOptions(); });

    bar->addSeparator();
    bar->addAction(m_actGrid);
    bar->addAction(m_actList);

    m_zoom = new QSlider(Qt::Horizontal, bar);
    m_zoom->setRange(Settings::kMinZoom, Settings::kMaxZoom);
    m_zoom->setValue(m_settings.zoom);
    m_zoom->setFixedWidth(110);
    m_zoom->setToolTip(tr("Thumbnail size"));
    m_zoom->setAccessibleName(tr("Thumbnail size"));
    bar->addWidget(m_zoom);
    connect(m_zoom, &QSlider::valueChanged, this, [this](int value) {
        m_settings.zoom = value;
        applyViewMode();
        m_saveTimer->start();
    });

    bar->addAction(m_actDetails);

    buildAppMenu();
    applyIcons();
}

// Everything that is not about one file lives in this menu. It opens with a right click on the
// background, the sidebar or the toolbar
void MainWindow::buildAppMenu()
{
    m_appMenu = new QMenu(this);

    QAction *add = m_appMenu->addAction(icon("list-add"), tr("Add Files to Favorites..."));
    add->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    connect(add, &QAction::triggered, this, &MainWindow::addFiles);
    addAction(add);
    m_appMenu->addAction(m_actQuick);
    m_appMenu->addSeparator();

    m_appMenu->addMenu(m_sortMenu);
    QMenu *view = m_appMenu->addMenu(tr("View"));
    view->addAction(m_actGrid);
    view->addAction(m_actList);
    view->addAction(m_actDetails);
    m_appMenu->addSeparator();

    QAction *groups = m_appMenu->addAction(icon("view-list-tree"), tr("Groups..."));
    connect(groups, &QAction::triggered, this, &MainWindow::openGroups);
    QAction *settings = m_appMenu->addAction(icon("configure"), tr("Settings..."));
    settings->setShortcut(QKeySequence::Preferences);
    connect(settings, &QAction::triggered, this, &MainWindow::openSettings);
    addAction(settings);
    m_appMenu->addSeparator();

    m_appMenu->addAction(m_actRefresh);
    QAction *shortcuts = m_appMenu->addAction(tr("Keyboard Shortcuts"));
    shortcuts->setShortcut(QKeySequence(Qt::Key_F1));
    connect(shortcuts, &QAction::triggered, this, &MainWindow::showShortcuts);
    addAction(shortcuts);
    QAction *about = m_appMenu->addAction(icon("help-about"), tr("About Sheaf"));
    connect(about, &QAction::triggered, this, &MainWindow::showAbout);
    m_appMenu->addSeparator();
    QAction *quit = m_appMenu->addAction(icon("application-exit"), tr("Quit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    addAction(quit);
    auto *closeWindow = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_W), this);
    connect(closeWindow, &QShortcut::activated, this, &QWidget::close);
}

void MainWindow::applyIcons()
{
    const QPalette pal = palette();
    m_actGrid->setIcon(glyphIcon(Glyph::Grid, pal));
    m_actList->setIcon(glyphIcon(Glyph::List, pal));
    m_actDetails->setIcon(glyphIcon(Glyph::Details, pal));
    m_sortButton->setIcon(glyphIcon(Glyph::Sort, pal));
    m_searchIcon->setIcon(glyphIcon(Glyph::Search, pal));
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && m_sortButton) {
        applyIcons();
    }
}

void MainWindow::applyViewMode()
{
    const bool grid = m_settings.gridView;
    m_delegate->setMode(grid ? CardDelegate::Mode::Grid : CardDelegate::Mode::List);
    m_delegate->setZoom(m_settings.zoom);
    m_zoom->setEnabled(grid);
    QStyleOptionViewItem option;
    option.font = m_view->font();
    if (grid) {
        m_view->setViewMode(QListView::IconMode);
        m_view->setMovement(QListView::Static);
        m_view->setResizeMode(QListView::Adjust);
        m_view->setWrapping(true);
        m_view->setFlow(QListView::LeftToRight);
        m_view->setSpacing(2);
        m_view->setGridSize(m_delegate->sizeHint(option, QModelIndex()));
        m_view->setDragEnabled(true);
    } else {
        m_view->setViewMode(QListView::ListMode);
        m_view->setMovement(QListView::Static);
        m_view->setResizeMode(QListView::Adjust);
        m_view->setFlow(QListView::TopToBottom);
        m_view->setWrapping(false);
        m_view->setSpacing(0);
        m_view->setGridSize(QSize());
        m_view->setDragEnabled(true);
    }
    m_view->setDragDropMode(QAbstractItemView::DragOnly);
    // Uniform sizes are measured once, so switch it off and on to measure again for the new mode
    m_view->setUniformItemSizes(false);
    m_view->setUniformItemSizes(true);
    m_view->doItemsLayout();
    m_view->viewport()->update();
}

void MainWindow::setGridView(bool grid)
{
    (grid ? m_actGrid : m_actList)->setChecked(true);
    m_settings.gridView = grid;
    applyViewMode();
}

void MainWindow::applySortOptions()
{
    SortOptions options;
    if (const QAction *checked = m_sortGroup->checkedAction()) {
        options.mode = static_cast<SortMode>(checked->data().toInt());
    }
    options.reversed = m_actReverse->isChecked();
    options.favoritesFirst = m_actFavoritesFirst->isChecked();
    m_settings.sort = options;
    m_proxy->setSortOptions(options);
    m_saveTimer->start();
}

void MainWindow::onDocumentsChanged()
{
    // Keep the selection and the scroll position across a refresh
    QSet<QString> selected;
    for (const QModelIndex &index : m_view->selectionModel()->selectedIndexes()) {
        selected.insert(index.data(DocumentModel::KeyRole).toString());
    }
    const QString current = m_view->currentIndex().data(DocumentModel::KeyRole).toString();
    const int scroll = m_view->verticalScrollBar()->value();

    m_model->setDocuments(m_index->documents());
    rebuildSidebar();
    applyFilter();

    if (!selected.isEmpty() || !current.isEmpty()) {
        QItemSelection selection;
        QModelIndex currentIndex;
        for (int row = 0; row < m_proxy->rowCount(); ++row) {
            const QModelIndex index = m_proxy->index(row, 0);
            const QString key = index.data(DocumentModel::KeyRole).toString();
            if (selected.contains(key)) {
                selection.select(index, index);
            }
            if (key == current) {
                currentIndex = index;
            }
        }
        m_view->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
        if (currentIndex.isValid()) {
            m_view->selectionModel()->setCurrentIndex(currentIndex, QItemSelectionModel::NoUpdate);
        }
    }
    m_view->verticalScrollBar()->setValue(scroll);
    updateEmptyState();
    updateStatus();
    onSelectionChanged();
}

void MainWindow::rebuildSidebar()
{
    m_rebuilding = true;
    QSignalBlocker blocker(m_sidebar);
    m_sidebar->clear();
    const QVector<DocumentInfo> &docs = m_index->documents();

    const QVector<Group> groups = m_groups.visibleGroups();
    for (const Group &group : groups) {
        int count = 0;
        for (const DocumentInfo &doc : docs) {
            if (!doc.exists && !m_settings.showMissing) {
                continue;
            }
            if (groupMatches(group, doc)) {
                ++count;
            }
        }
        m_sidebar->addGroup(group.id, group.displayTitle(), groupIcon(group), count);
    }

    QMap<QString, int> tagCounts;
    for (const DocumentInfo &doc : docs) {
        if (!doc.exists && !m_settings.showMissing) {
            continue;
        }
        for (const QString &tag : doc.tags) {
            ++tagCounts[tag];
        }
    }
    if (!tagCounts.isEmpty()) {
        m_sidebar->addHeader(tr("Tags"));
        for (auto it = tagCounts.constBegin(); it != tagCounts.constEnd(); ++it) {
            m_sidebar->addTag(it.key(), it.value());
        }
    }

    bool selected = false;
    if (!m_activeTag.isEmpty()) {
        selected = m_sidebar->selectEntry(Sidebar::TagKind, m_activeTag);
        if (!selected) {
            m_activeTag.clear();
        }
    }
    if (!selected) {
        selected = m_sidebar->selectEntry(Sidebar::GroupKind, m_activeGroup);
    }
    if (!selected) {
        m_activeGroup = QStringLiteral("all");
        m_sidebar->selectEntry(Sidebar::GroupKind, m_activeGroup);
    }
    m_rebuilding = false;
}

void MainWindow::onSidebarActivated(int kind, const QString &id)
{
    if (m_rebuilding) {
        return;
    }
    if (kind == Sidebar::TagKind) {
        m_activeTag = id;
    } else {
        m_activeTag.clear();
        m_activeGroup = id;
        m_groups.setActiveGroup(id);
        m_saveTimer->start();
    }
    // Picking something in the sidebar ends a search
    if (!m_search->text().isEmpty()) {
        QSignalBlocker block(m_search);
        m_search->clear();
    }
    applyFilter();
    updateEmptyState();
    updateStatus();
}

void MainWindow::applyFilter()
{
    if (!m_search->text().trimmed().isEmpty()) {
        // A search looks through the whole library
        m_proxy->setGroup(everythingGroup());
    } else if (!m_activeTag.isEmpty()) {
        m_proxy->setTagFilter(m_activeTag);
    } else {
        bool found = false;
        const Group group = m_groups.groupById(m_activeGroup, &found);
        m_proxy->setGroup(found ? group : everythingGroup());
    }
    m_proxy->setQuery(m_search->text());
}

int MainWindow::hiddenMissingCount() const
{
    if (m_settings.showMissing) {
        return 0;
    }
    int missing = 0;
    for (const DocumentInfo &doc : m_index->documents()) {
        if (!doc.exists) {
            ++missing;
        }
    }
    return missing;
}

void MainWindow::updateEmptyState()
{
    const bool hasRows = m_proxy->rowCount() > 0;
    m_stack->setCurrentWidget(hasRows ? static_cast<QWidget *>(m_view) : m_empty);
    if (hasRows) {
        return;
    }

    // Short, friendly lines. Each kind of file gets its own
    struct Message {
        const char *id;
        const char *title;
        const char *text;
    };
    static const Message messages[] = {
        {"favorites", QT_TR_NOOP("No favorites yet"), QT_TR_NOOP("Hover over a file and click its star, or press Ctrl+D, to keep it close.")},
        {"all", QT_TR_NOOP("A clean slate"), QT_TR_NOOP("Files you open in other apps will turn up here on their own.")},
        {"text-files", QT_TR_NOOP("Nothing to read yet"), QT_TR_NOOP("Open a note or a text file and it will appear here.")},
        {"documents", QT_TR_NOOP("No PDFs in sight"), QT_TR_NOOP("Open a PDF in any app and it will land here.")},
        {"books", QT_TR_NOOP("An empty bookshelf"), QT_TR_NOOP("Open an ebook and it will take its place on the shelf.")},
        {"writer-documents", QT_TR_NOOP("A blank page"), QT_TR_NOOP("Letters, reports and other word processor files will be listed here.")},
        {"spreadsheets", QT_TR_NOOP("No spreadsheets, no problem"), QT_TR_NOOP("Not a single cell in sight. Open one in any app and it will land here.")},
        {"presentations", QT_TR_NOOP("The stage is empty"), QT_TR_NOOP("Open a presentation and it will step up here.")},
        {"drawings", QT_TR_NOOP("A blank canvas"), QT_TR_NOOP("Open a drawing and it will be pinned up here.")},
        {"databases", QT_TR_NOOP("No databases"), QT_TR_NOOP("Open a database file and it will be listed here.")},
        {"pictures", QT_TR_NOOP("No pictures yet"), QT_TR_NOOP("Open an image and a thumbnail will show up here.")},
        {"audio", QT_TR_NOOP("All quiet"), QT_TR_NOOP("Open an audio file and it will be listed here.")},
        {"videos", QT_TR_NOOP("The screen is dark"), QT_TR_NOOP("Open a video and it will appear here.")},
        {"code", QT_TR_NOOP("No code yet"), QT_TR_NOOP("Open a source file in your editor and it will be listed here.")},
        {"archives", QT_TR_NOOP("No archives"), QT_TR_NOOP("Open a zip or another archive and it will be listed here.")},
    };

    m_emptyButton->setVisible(false);
    QString title;
    QString text;
    const int hidden = hiddenMissingCount();
    if (m_index->isBusy() && m_index->documents().isEmpty()) {
        title = tr("Looking around...");
        text = tr("Checking your recent files.");
    } else if (!m_search->text().trimmed().isEmpty()) {
        title = tr("Nothing matches that");
        text = tr("Try fewer words, or drop an operator such as tag: or type:.");
    } else if (m_index->documents().isEmpty()) {
        title = tr("A clean slate");
        text = tr("Files you open in other apps will turn up here on their own. You can also drop files onto this window to add them to your favorites.");
        m_emptyButton->setVisible(true);
    } else if (hidden > 0 && hidden == m_index->documents().size()) {
        title = tr("Everything has wandered off");
        text = tr("Every file on your recent list has been moved or deleted. Open something and it will show up here.");
    } else if (!m_activeTag.isEmpty()) {
        title = tr("Nothing tagged \"%1\"").arg(m_activeTag);
        text = tr("Select a file and press Ctrl+T to give it this tag.");
    } else {
        title = tr("Nothing here yet");
        text = tr("Files that match this group will show up on their own.");
        for (const Message &message : messages) {
            if (m_activeGroup == QLatin1String(message.id)) {
                title = tr(message.title);
                text = tr(message.text);
                break;
            }
        }
    }
    m_emptyTitle->setText(title);
    m_emptyText->setText(text);

    // A wrapping label inside a centred layout can end up too short, so give it its height explicitly
    const int width = qBound(220, m_empty->width() - 80, 440);
    m_emptyText->setFixedWidth(width);
    const QRect needed = m_emptyText->fontMetrics().boundingRect(QRect(0, 0, width, 4000), Qt::TextWordWrap | Qt::AlignHCenter, text);
    m_emptyText->setFixedHeight(text.isEmpty() ? 0 : needed.height() + 6);
}

void MainWindow::updateStatus()
{
    QString text = tr("%n file(s)", nullptr, m_proxy->rowCount());
    if (!m_search->text().trimmed().isEmpty()) {
        text += QStringLiteral("  ") + tr("(searching the whole library)");
    }
    m_status->setText(text);
}

void MainWindow::flash(const QString &message)
{
    statusBar()->showMessage(message, 8000);
}

QList<DocumentInfo> MainWindow::selectedDocuments() const
{
    QList<DocumentInfo> docs;
    QModelIndexList rows = m_view->selectionModel()->selectedIndexes();
    std::sort(rows.begin(), rows.end(), [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });
    for (const QModelIndex &index : rows) {
        docs << m_proxy->documentAt(index);
    }
    return docs;
}

void MainWindow::onSelectionChanged()
{
    const QList<DocumentInfo> docs = selectedDocuments();
    m_details->setDocuments(docs);
    updateActions();
}

void MainWindow::updateActions()
{
    const QList<DocumentInfo> docs = selectedDocuments();
    const bool any = !docs.isEmpty();
    const bool single = docs.size() == 1;
    bool anyExisting = false;
    bool allFavorite = any;
    for (const DocumentInfo &doc : docs) {
        anyExisting = anyExisting || doc.exists;
        allFavorite = allFavorite && doc.favorite;
    }
    m_actOpen->setEnabled(anyExisting);
    m_actFolder->setEnabled(single && docs.first().exists);
    m_actFavorite->setEnabled(any);
    m_actFavorite->setText(allFavorite ? tr("Remove from &Favorites") : tr("Add to &Favorites"));
    m_actTags->setEnabled(any);
    m_actRemove->setEnabled(any);
    m_actTrash->setEnabled(anyExisting);
    m_actCopyPath->setEnabled(any);
    m_actCopyFile->setEnabled(anyExisting);
    m_actLocate->setEnabled(single && !docs.first().exists);
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    const QModelIndex under = m_view->indexAt(pos);
    if (under.isValid() && !m_view->selectionModel()->isSelected(under)) {
        m_view->selectionModel()->select(under, QItemSelectionModel::ClearAndSelect);
        m_view->setCurrentIndex(under);
    }
    const QList<DocumentInfo> docs = selectedDocuments();
    if (docs.isEmpty() || !under.isValid()) {
        m_view->selectionModel()->clearSelection();
        m_appMenu->popup(m_view->viewport()->mapToGlobal(pos));
        return;
    }
    QMenu menu(this);
    menu.addAction(m_actOpen);
    menu.addAction(m_actFolder);
    menu.addSeparator();
    menu.addAction(m_actFavorite);
    menu.addAction(m_actTags);
    menu.addSeparator();
    menu.addAction(m_actCopyFile);
    menu.addAction(m_actCopyPath);
    if (docs.size() == 1 && !docs.first().exists) {
        menu.addAction(m_actLocate);
    }
    menu.addSeparator();
    menu.addAction(m_actRemove);
    menu.addAction(m_actTrash);
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void MainWindow::openDocuments(const QList<DocumentInfo> &docs)
{
    if (docs.size() > 10 &&
        QMessageBox::question(this, tr("Open Files"), tr("Open %n file(s) at once?", nullptr, docs.size())) != QMessageBox::Yes) {
        return;
    }
    for (const DocumentInfo &doc : docs) {
        switch (launcher::open(doc.path, doc.mime)) {
        case OpenResult::Ok:
            m_index->recordOpen(doc);
            break;
        case OpenResult::Missing:
            flash(tr("%1 was not found. Use Locate File if it moved.").arg(doc.name));
            m_index->refresh();
            break;
        case OpenResult::Blocked:
            QMessageBox::warning(this, tr("Sheaf"),
                                 tr("%1 is a program, so Sheaf does not start it. You can still show it in its folder.").arg(doc.name));
            break;
        case OpenResult::Failed:
            QMessageBox::warning(this, tr("Sheaf"),
                                 tr("Could not open %1. Check that an application is set for this type of file.").arg(doc.name));
            break;
        }
    }
}

void MainWindow::showInFolder()
{
    const QList<DocumentInfo> docs = selectedDocuments();
    if (docs.size() != 1) {
        return;
    }
    const QString path = docs.first().path;
    (void) QtConcurrent::run([path]() { launcher::showInFolder(path); });
}

void MainWindow::toggleFavorite()
{
    const QList<DocumentInfo> docs = selectedDocuments();
    bool allFavorite = !docs.isEmpty();
    for (const DocumentInfo &doc : docs) {
        allFavorite = allFavorite && doc.favorite;
    }
    for (const DocumentInfo &doc : docs) {
        m_index->setFavorite(doc, !allFavorite);
    }
    flash(allFavorite ? tr("Removed from favorites") : tr("Added to favorites"));
}

void MainWindow::editTags()
{
    const QList<DocumentInfo> docs = selectedDocuments();
    if (docs.isEmpty()) {
        return;
    }
    const bool single = docs.size() == 1;
    TagDialog dialog(single ? docs.first().tags : QStringList(), m_db.allTagNames(), !single, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QStringList tags = dialog.tags();
    for (const DocumentInfo &doc : docs) {
        if (single) {
            m_index->setTags(doc, tags);
        } else {
            QStringList merged = doc.tags;
            for (const QString &tag : tags) {
                if (!merged.contains(tag, Qt::CaseInsensitive)) {
                    merged << tag;
                }
            }
            m_index->setTags(doc, merged);
        }
    }
}

void MainWindow::removeFromList()
{
    const QList<DocumentInfo> docs = selectedDocuments();
    if (docs.isEmpty()) {
        return;
    }
    QString error;
    if (!m_index->removeFromRecents(docs, &error) && !error.isEmpty()) {
        QMessageBox::warning(this, tr("Sheaf"), tr("The recent files list could not be changed.\n\n%1").arg(error));
        return;
    }
    flash(tr("Removed from the recent list"));
}

void MainWindow::moveToTrash()
{
    const QList<DocumentInfo> docs = selectedDocuments();
    if (docs.isEmpty()) {
        return;
    }
    if (m_settings.confirmTrash) {
        QMessageBox box(QMessageBox::Warning, tr("Move to Trash"),
                        docs.size() == 1 ? tr("Move \"%1\" to the trash?").arg(docs.first().name) : tr("Move %n file(s) to the trash?", nullptr, docs.size()),
                        QMessageBox::Cancel, this);
        box.setInformativeText(tr("The files are moved, not deleted. You can restore them from the trash."));
        QPushButton *confirm = box.addButton(tr("Move to Trash"), QMessageBox::DestructiveRole);
        box.setDefaultButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() != confirm) {
            return;
        }
    }
    QList<DocumentInfo> done;
    int failed = 0;
    for (const DocumentInfo &doc : docs) {
        if (!doc.exists || launcher::moveToTrash(doc.path)) {
            done << doc;
        } else {
            ++failed;
        }
    }
    m_index->removeEverything(done);
    flash(failed > 0 ? tr("%n file(s) could not be moved to the trash", nullptr, failed) : tr("Moved to the trash"));
}

void MainWindow::copyPath()
{
    QStringList lines;
    for (const DocumentInfo &doc : selectedDocuments()) {
        lines << doc.path;
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    flash(tr("Path copied"));
}

void MainWindow::copyFile()
{
    QList<QUrl> urls;
    QByteArray gnome = "copy";
    for (const DocumentInfo &doc : selectedDocuments()) {
        if (doc.exists) {
            const QUrl url = QUrl::fromLocalFile(doc.path);
            urls << url;
            gnome += '\n' + url.toEncoded();
        }
    }
    if (urls.isEmpty()) {
        return;
    }
    auto *mime = new QMimeData;
    mime->setUrls(urls);
    mime->setData(QStringLiteral("x-special/gnome-copied-files"), gnome);
    mime->setData(QStringLiteral("application/x-kde-cutselection"), "0");
    QApplication::clipboard()->setMimeData(mime);
    flash(tr("File copied, paste it in a file manager"));
}

void MainWindow::locateFile()
{
    const QList<DocumentInfo> docs = selectedDocuments();
    if (docs.size() != 1) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Locate %1").arg(docs.first().name), docs.first().folder());
    if (!path.isEmpty() && m_index->relocate(docs.first(), path)) {
        flash(tr("File moved to its new location"));
    }
}

void MainWindow::addFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Add Files to Favorites"));
    if (!files.isEmpty()) {
        addFavoritePaths(files);
    }
}

void MainWindow::addFavoritePaths(const QStringList &filePaths)
{
    const QStringList mounts = pathutil::networkMountPoints();
    int added = 0;
    for (const QString &path : filePaths) {
        const QFileInfo info(path);
        if (!info.isFile()) {
            continue;
        }
        DocumentInfo doc;
        doc.key = documentKey(path, mounts);
        doc.path = doc.key;
        doc.uri = pathutil::uriFromPath(doc.key);
        if (m_index->setFavorite(doc, true)) {
            ++added;
        }
    }
    flash(added > 0 ? tr("%n file(s) added to favorites", nullptr, added) : tr("Only files can be added"));
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            files << url.toLocalFile();
        }
    }
    if (!files.isEmpty()) {
        addFavoritePaths(files);
        event->acceptProposedAction();
    }
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(m_settings, progressSupported(), this);
    connect(&dialog, &SettingsDialog::importThingyRequested, this, &MainWindow::importFromThingy);
    connect(&dialog, &SettingsDialog::exportRequested, this, &MainWindow::exportLibrary);
    connect(&dialog, &SettingsDialog::importRequested, this, &MainWindow::importLibrary);
    connect(&dialog, &SettingsDialog::clearRecentsRequested, this, &MainWindow::clearRecents);
    connect(&dialog, &SettingsDialog::removeMissingRequested, this, &MainWindow::removeMissing);
    if (dialog.exec() == QDialog::Accepted) {
        applySettings(dialog.settings());
    }
}

void MainWindow::applySettings(const Settings &settings)
{
    m_settings = settings;
    m_loader->setGenerateEnabled(m_settings.generateThumbnails);
    m_settings.showMissing = false;
    m_proxy->setHideMissing(true);
    m_settings.save(paths::settingsFile());
    rebuildSidebar();
    updateEmptyState();
    updateStatus();
    m_index->setOptions(scanOptions());
}

void MainWindow::openGroups()
{
    GroupsDialog dialog(m_groups, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_groups = dialog.config();
    QString error;
    if (!m_groups.save(paths::groupsFile(), &error)) {
        flash(tr("Could not save the groups: %1").arg(error));
    }
    rebuildSidebar();
    applyFilter();
    updateEmptyState();
}

void MainWindow::importFromThingy()
{
    int favorites = 0;
    int groups = 0;
    QStringList uris;
    QString error;
    if (thingy::readFavorites(&uris, &error)) {
        const QStringList mounts = pathutil::networkMountPoints();
        for (const QString &uri : uris) {
            const QString path = pathutil::pathFromUri(uri);
            if (path.isEmpty() || !QFileInfo(path).isFile()) {
                continue;
            }
            DocumentInfo doc;
            doc.key = documentKey(path, mounts);
            doc.path = doc.key;
            doc.uri = uri;
            if (m_index->setFavorite(doc, true)) {
                ++favorites;
            }
        }
    }
    if (QFileInfo::exists(thingy::groupsFile())) {
        const int imported = m_groups.importThingy(thingy::groupsFile());
        if (imported >= 0) {
            groups = imported;
            m_groups.save(paths::groupsFile());
            rebuildSidebar();
        }
    }
    if (favorites == 0 && groups == 0 && !error.isEmpty() && !QFileInfo::exists(thingy::groupsFile())) {
        QMessageBox::information(this, tr("Import from Thingy"), tr("No Thingy data was found on this system."));
        return;
    }
    QMessageBox::information(this, tr("Import from Thingy"),
                             tr("Imported %n favorites", nullptr, favorites) + QStringLiteral("\n") + tr("Imported %n custom groups", nullptr, groups));
}

void MainWindow::exportLibrary()
{
    const QString file = QFileDialog::getSaveFileName(this, tr("Export Library"), QStringLiteral("sheaf-library.json"), tr("JSON files (*.json)"));
    if (file.isEmpty()) {
        return;
    }
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(m_db.exportJson()).toJson(QJsonDocument::Indented)) < 0 || !out.commit()) {
        QMessageBox::warning(this, tr("Export Library"), tr("The file could not be written."));
        return;
    }
    flash(tr("Library exported"));
}

void MainWindow::importLibrary()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Import Library"), QString(), tr("JSON files (*.json)"));
    if (file.isEmpty()) {
        return;
    }
    QFile in(file);
    QJsonParseError parseError;
    QJsonDocument doc;
    if (in.open(QIODevice::ReadOnly) && in.size() < 64 * 1024 * 1024) {
        doc = QJsonDocument::fromJson(in.readAll(), &parseError);
    }
    int count = 0;
    QString error;
    if (!doc.isObject() || !m_db.importJson(doc.object(), &count, &error)) {
        QMessageBox::warning(this, tr("Import Library"), tr("This is not a library file made by Sheaf."));
        return;
    }
    m_index->refresh();
    flash(tr("%n items imported", nullptr, count));
}

void MainWindow::clearRecents()
{
    QMessageBox box(QMessageBox::Warning, tr("Clear Recent Files"), tr("Clear all recent files?"), QMessageBox::Cancel, this);
    box.setInformativeText(tr("This clears the history of recent files for all applications. Favorites are kept."));
    QPushButton *confirm = box.addButton(tr("Clear"), QMessageBox::DestructiveRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != confirm) {
        return;
    }
    QString error;
    if (!m_index->clearRecents(&error)) {
        QMessageBox::warning(this, tr("Sheaf"), tr("The recent files list could not be cleared.\n\n%1").arg(error));
        return;
    }
    flash(tr("Recent files cleared"));
}

void MainWindow::removeMissing()
{
    const int removed = m_index->removeMissing();
    flash(removed > 0 ? tr("%n moved or deleted file(s) forgotten", nullptr, removed) : tr("No files are missing"));
}

void MainWindow::showShortcuts()
{
    const QString text = tr(
        "<table cellpadding='4'>"
        "<tr><td><b>Enter</b></td><td>Open the selected documents</td></tr>"
        "<tr><td><b>Ctrl+F</b> or <b>/</b></td><td>Search the whole library</td></tr>"
        "<tr><td><b>Ctrl+K</b></td><td>Quick open</td></tr>"
        "<tr><td><b>Ctrl+D</b></td><td>Add to or remove from favorites</td></tr>"
        "<tr><td><b>Ctrl+T</b></td><td>Edit tags</td></tr>"
        "<tr><td><b>Ctrl+Shift+O</b></td><td>Show in folder</td></tr>"
        "<tr><td><b>Ctrl+C</b></td><td>Copy the file</td></tr>"
        "<tr><td><b>Ctrl+Shift+C</b></td><td>Copy the path</td></tr>"
        "<tr><td><b>Delete</b></td><td>Remove from the recent list</td></tr>"
        "<tr><td><b>Shift+Delete</b></td><td>Move to trash</td></tr>"
        "<tr><td><b>Ctrl+1 to 9</b></td><td>Go to a group</td></tr>"
        "<tr><td><b>Ctrl+Shift+G</b> / <b>L</b></td><td>Grid or list view</td></tr>"
        "<tr><td><b>Ctrl+wheel</b></td><td>Change thumbnail size</td></tr>"
        "<tr><td><b>F9</b></td><td>Show or hide details</td></tr>"
        "<tr><td><b>F5</b></td><td>Refresh</td></tr>"
        "<tr><td><b>Ctrl+Q</b></td><td>Quit</td></tr>"
        "</table>"
        "<p>Search operators: <b>tag:work</b> <b>type:pdf</b> <b>in:invoices</b> <b>is:fav</b> <b>is:missing</b> <b>-draft</b></p>");
    QMessageBox::information(this, tr("Keyboard Shortcuts"), text);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About Sheaf"),
                       tr("<h3>Sheaf %1</h3>"
                          "<p>A library for your recent and favorite documents.</p>"
                          "<p>Released under the MIT License.</p>")
                           .arg(QString::fromLatin1(SHEAF_VERSION)));
}

void MainWindow::showQuickOpen()
{
    m_quick->popup();
}

void MainWindow::raiseWindow()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::saveState()
{
    m_settings.geometry = saveGeometry();
    m_settings.splitterState = m_splitter->saveState();
    m_settings.save(paths::settingsFile());
    m_groups.save(paths::groupsFile());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_saveTimer) {
        m_saveTimer->start();
    }
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    if (m_saveTimer) {
        m_saveTimer->start();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_ready) {
        saveState();
    }
    m_quick->hide();
    event->accept();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_empty && event->type() == QEvent::Resize) {
        updateEmptyState();
    }
    if (watched == m_view->viewport() && event->type() == QEvent::MouseButtonPress) {
        // A click on the star of a file turns it into a favorite, or back
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::NoModifier) {
            const QModelIndex index = m_view->indexAt(mouse->position().toPoint());
            if (index.isValid()) {
                QStyleOptionViewItem option;
                option.rect = m_view->visualRect(index);
                if (m_delegate->starRect(option).contains(mouse->position().toPoint())) {
                    const DocumentInfo doc = m_proxy->documentAt(index);
                    m_index->setFavorite(doc, !doc.favorite);
                    return true;
                }
            }
        }
    }
    if (watched == m_view->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            const int step = wheel->angleDelta().y() > 0 ? 16 : -16;
            m_zoom->setValue(m_zoom->value() + step);
            return true;
        }
    }
    if (watched == m_view && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        const bool plain = !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        if (plain && !key->text().isEmpty() && key->text().at(0).isPrint() && key->key() != Qt::Key_Space) {
            // Typing in the list starts a search, like in most modern file pickers
            m_search->setFocus();
            m_search->setText(m_search->text() + key->text());
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            openDocuments(selectedDocuments());
            return true;
        }
    }
    if (watched == m_search && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            if (!m_search->text().isEmpty()) {
                m_search->clear();
            }
            m_view->setFocus();
            return true;
        }
        if (key->key() == Qt::Key_Down) {
            m_view->setFocus();
            if (!m_view->currentIndex().isValid()) {
                m_view->setCurrentIndex(m_proxy->index(0, 0));
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

} // namespace gui
} // namespace sheaf
