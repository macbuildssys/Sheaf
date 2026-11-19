#pragma once

#include "groups.h"

#include <QDialog>

class QListWidget;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QLabel;

namespace sheaf {
namespace gui {

// Shows, hides, orders and edits the groups in the sidebar
class GroupsDialog : public QDialog {
    Q_OBJECT
public:
    GroupsDialog(const GroupsConfig &config, QWidget *parent = nullptr);

    GroupsConfig config() const { return m_config; }

private:
    void reload(const QString &select = QString());
    void onItemChanged();
    void addGroup();
    void editGroup();
    void removeGroup();
    void moveSelected(int delta);
    void resetAll();
    QString selectedId() const;

    GroupsConfig m_config;
    QListWidget *m_list;
    QPushButton *m_edit;
    QPushButton *m_remove;
    QPushButton *m_up;
    QPushButton *m_down;
    bool m_loading = false;
};

class GroupEditor : public QDialog {
    Q_OBJECT
public:
    GroupEditor(const Group &group, QWidget *parent = nullptr);

    Group group() const;

private:
    void addPattern();
    void addFromFiles();
    void removePattern();
    void browseFolder();
    void accept() override;

    Group m_group;
    QLineEdit *m_name;
    QLineEdit *m_icon;
    QLabel *m_iconPreview;
    QListWidget *m_patterns;
    QLineEdit *m_patternEntry;
    QCheckBox *m_useFolder;
    QLineEdit *m_folder;
    QPushButton *m_browse;
    QLineEdit *m_tags;
    QLabel *m_error;
};

} // namespace gui
} // namespace sheaf
