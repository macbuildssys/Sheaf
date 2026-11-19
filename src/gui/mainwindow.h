#pragma once

#include "database.h"
#include "documentmodel.h"
#include "groups.h"
#include "index.h"
#include "settings.h"

#include <QList>
#include <QMainWindow>

class QAction;
class QActionGroup;
class QLabel;
class QLineEdit;
class QListView;
class QMenu;
class QSlider;
class QSplitter;
class QStackedWidget;
class QToolButton;
class QTimer;
class QPushButton;

namespace sheaf {
namespace gui {

class CardDelegate;
class DetailsPanel;
class QuickOpen;
class Sidebar;
class ThumbnailLoader;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // False when the library database could not be opened
    bool isReady() const { return m_ready; }
    int documentCount() const { return m_proxy->rowCount(); }

    // Test and tooling access
    DocumentIndex *index() { return m_index; }
    DocumentFilterProxy *proxy() { return m_proxy; }
    QListView *view() { return m_view; }
    Sidebar *sidebar() { return m_sidebar; }
    void setGridView(bool grid);
    void setDetailsVisible(bool visible) { m_actDetails->setChecked(visible); }

public slots:
    void showQuickOpen();
    void raiseWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildActions();
    void buildToolbar();
    void buildAppMenu();
    void applyIcons();
    void onDocumentsChanged();
    void rebuildSidebar();
    void onSidebarActivated(int kind, const QString &id);
    void applyFilter();
    void applyViewMode();
    void applySortOptions();
    void updateEmptyState();
    int hiddenMissingCount() const;
    void updateStatus();
    void updateActions();
    void onSelectionChanged();
    void showContextMenu(const QPoint &pos);

    QList<DocumentInfo> selectedDocuments() const;
    void openDocuments(const QList<DocumentInfo> &docs);
    void showInFolder();
    void toggleFavorite();
    void editTags();
    void removeFromList();
    void moveToTrash();
    void copyPath();
    void copyFile();
    void locateFile();
    void addFiles();
    void addFavoritePaths(const QStringList &paths);
    void openSettings();
    void openGroups();
    void importFromThingy();
    void exportLibrary();
    void importLibrary();
    void clearRecents();
    void removeMissing();
    void showShortcuts();
    void showAbout();
    void saveState();
    void flash(const QString &message);
    ScanOptions scanOptions() const;
    void applySettings(const Settings &settings);

    Database m_db;
    Settings m_settings;
    GroupsConfig m_groups;
    DocumentIndex *m_index = nullptr;
    DocumentModel *m_model = nullptr;
    DocumentFilterProxy *m_proxy = nullptr;
    ThumbnailLoader *m_loader = nullptr;
    CardDelegate *m_delegate = nullptr;
    QuickOpen *m_quick = nullptr;

    QSplitter *m_splitter = nullptr;
    Sidebar *m_sidebar = nullptr;
    QStackedWidget *m_stack = nullptr;
    QListView *m_view = nullptr;
    QWidget *m_empty = nullptr;
    QLabel *m_emptyTitle = nullptr;
    QLabel *m_emptyText = nullptr;
    QPushButton *m_emptyButton = nullptr;
    DetailsPanel *m_details = nullptr;
    QLineEdit *m_search = nullptr;
    QSlider *m_zoom = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_busy = nullptr;

    QAction *m_actOpen = nullptr;
    QAction *m_actFolder = nullptr;
    QAction *m_actFavorite = nullptr;
    QAction *m_actTags = nullptr;
    QAction *m_actRemove = nullptr;
    QAction *m_actTrash = nullptr;
    QAction *m_actCopyPath = nullptr;
    QAction *m_actCopyFile = nullptr;
    QAction *m_actLocate = nullptr;
    QAction *m_actGrid = nullptr;
    QAction *m_actList = nullptr;
    QAction *m_actDetails = nullptr;
    QAction *m_actFavoritesFirst = nullptr;
    QAction *m_actReverse = nullptr;
    QAction *m_actRefresh = nullptr;
    QAction *m_actQuick = nullptr;
    QAction *m_searchIcon = nullptr;
    QActionGroup *m_sortGroup = nullptr;
    QToolButton *m_sortButton = nullptr;
    QMenu *m_sortMenu = nullptr;
    QMenu *m_appMenu = nullptr;

    QTimer *m_saveTimer = nullptr;
    bool m_ready = false;
    bool m_rebuilding = false;
    bool m_quickOnly = false;
    QString m_activeGroup;
    QString m_activeTag;

public:
    // With quick only the main window stays hidden and the process ends when the popup closes
    void setQuickOnly(bool quickOnly) { m_quickOnly = quickOnly; }
};

} // namespace gui
} // namespace sheaf
