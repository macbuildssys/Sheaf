#include "groupsdialog.h"

#include "guiutil.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPushButton>
#include <QVBoxLayout>

namespace sheaf {
namespace gui {

GroupsDialog::GroupsDialog(const GroupsConfig &config, QWidget *parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle(tr("Groups"));
    resize(520, 480);

    auto *layout = new QVBoxLayout(this);
    auto *hint = new QLabel(tr("Tick the groups that should appear in the sidebar. Custom groups can match file types, a folder and tags."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setAccessibleName(tr("Groups"));
    layout->addWidget(m_list, 1);

    auto *row = new QHBoxLayout;
    auto *add = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")), tr("Add..."), this);
    m_edit = new QPushButton(QIcon::fromTheme(QStringLiteral("document-edit")), tr("Edit..."), this);
    m_remove = new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")), tr("Remove"), this);
    m_up = new QPushButton(QIcon::fromTheme(QStringLiteral("go-up")), tr("Move Up"), this);
    m_down = new QPushButton(QIcon::fromTheme(QStringLiteral("go-down")), tr("Move Down"), this);
    auto *reset = new QPushButton(tr("Reset to Defaults"), this);
    row->addWidget(add);
    row->addWidget(m_edit);
    row->addWidget(m_remove);
    row->addStretch(1);
    row->addWidget(m_up);
    row->addWidget(m_down);
    layout->addLayout(row);
    layout->addWidget(reset, 0, Qt::AlignLeft);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(add, &QPushButton::clicked, this, &GroupsDialog::addGroup);
    connect(m_edit, &QPushButton::clicked, this, &GroupsDialog::editGroup);
    connect(m_remove, &QPushButton::clicked, this, &GroupsDialog::removeGroup);
    connect(m_up, &QPushButton::clicked, this, [this]() { moveSelected(-1); });
    connect(m_down, &QPushButton::clicked, this, [this]() { moveSelected(1); });
    connect(reset, &QPushButton::clicked, this, &GroupsDialog::resetAll);
    connect(m_list, &QListWidget::currentRowChanged, this, [this]() { onItemChanged(); });
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (!m_loading) {
            m_config.setHidden(item->data(Qt::UserRole).toString(), item->checkState() != Qt::Checked);
        }
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this]() { editGroup(); });
    reload();
}

QString GroupsDialog::selectedId() const
{
    const QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void GroupsDialog::reload(const QString &select)
{
    m_loading = true;
    m_list->clear();
    const QVector<Group> groups = m_config.allGroups();
    for (const Group &group : groups) {
        auto *item = new QListWidgetItem(groupIcon(group), group.displayTitle(), m_list);
        item->setData(Qt::UserRole, group.id);
        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        // The first two groups are always shown so that nothing can get lost
        if (group.id != QLatin1String("favorites") && group.id != QLatin1String("all")) {
            flags |= Qt::ItemIsUserCheckable;
            item->setCheckState(group.hidden ? Qt::Unchecked : Qt::Checked);
        }
        item->setFlags(flags);
        if (group.id == select) {
            m_list->setCurrentItem(item);
        }
    }
    m_loading = false;
    if (!m_list->currentItem() && m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    onItemChanged();
}

void GroupsDialog::onItemChanged()
{
    const QString id = selectedId();
    const bool custom = id.startsWith(QLatin1String("custom-"));
    m_edit->setEnabled(custom);
    m_remove->setEnabled(custom);
    m_up->setEnabled(m_list->currentRow() > 0);
    m_down->setEnabled(m_list->currentRow() >= 0 && m_list->currentRow() < m_list->count() - 1);
}

void GroupsDialog::addGroup()
{
    Group blank;
    blank.builtin = false;
    blank.iconNames = {QStringLiteral("folder")};
    GroupEditor editor(blank, this);
    if (editor.exec() == QDialog::Accepted) {
        const QString id = m_config.addCustom(editor.group());
        reload(id);
    }
}

void GroupsDialog::editGroup()
{
    const QString id = selectedId();
    bool found = false;
    const Group group = m_config.groupById(id, &found);
    if (!found || group.builtin) {
        return;
    }
    GroupEditor editor(group, this);
    if (editor.exec() == QDialog::Accepted) {
        Group updated = editor.group();
        updated.id = id;
        m_config.updateCustom(updated);
        reload(id);
    }
}

void GroupsDialog::removeGroup()
{
    const QString id = selectedId();
    bool found = false;
    const Group group = m_config.groupById(id, &found);
    if (!found || group.builtin) {
        return;
    }
    if (QMessageBox::question(this, tr("Remove Group"), tr("Remove the group \"%1\"? Your files are not affected.").arg(group.title)) ==
        QMessageBox::Yes) {
        m_config.removeCustom(id);
        reload();
    }
}

void GroupsDialog::moveSelected(int delta)
{
    const QString id = selectedId();
    m_config.move(id, delta);
    reload(id);
}

void GroupsDialog::resetAll()
{
    if (QMessageBox::question(this, tr("Reset Groups"), tr("Reset the group list to its defaults and remove custom groups?")) == QMessageBox::Yes) {
        m_config.resetToDefaults();
        reload();
    }
}

GroupEditor::GroupEditor(const Group &group, QWidget *parent)
    : QDialog(parent)
    , m_group(group)
{
    setWindowTitle(group.title.isEmpty() ? tr("New Group") : tr("Edit Group"));
    resize(480, 520);
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_name = new QLineEdit(group.title, this);
    m_name->setAccessibleName(tr("Name"));
    form->addRow(tr("&Name"), m_name);

    auto *iconRow = new QHBoxLayout;
    m_icon = new QLineEdit(group.iconName(), this);
    m_icon->setAccessibleName(tr("Icon name"));
    m_icon->setToolTip(tr("A name from your icon theme, such as folder or x-office-document"));
    m_iconPreview = new QLabel(this);
    m_iconPreview->setFixedSize(24, 24);
    iconRow->addWidget(m_icon, 1);
    iconRow->addWidget(m_iconPreview);
    form->addRow(tr("&Icon"), iconRow);
    auto updateIcon = [this]() { m_iconPreview->setPixmap(themeIcon({m_icon->text().trimmed()}).pixmap(24, 24)); };
    connect(m_icon, &QLineEdit::textChanged, this, updateIcon);
    updateIcon();

    m_patterns = new QListWidget(this);
    m_patterns->setAccessibleName(tr("File types"));
    m_patterns->addItems(group.patterns);
    form->addRow(tr("File &types"), m_patterns);
    auto *patternRow = new QHBoxLayout;
    m_patternEntry = new QLineEdit(this);
    m_patternEntry->setPlaceholderText(tr("application/pdf or image/*"));
    m_patternEntry->setAccessibleName(tr("File type to add"));
    auto *add = new QPushButton(tr("Add"), this);
    auto *fromFiles = new QPushButton(tr("From Files..."), this);
    auto *remove = new QPushButton(tr("Remove"), this);
    patternRow->addWidget(m_patternEntry, 1);
    patternRow->addWidget(add);
    patternRow->addWidget(fromFiles);
    patternRow->addWidget(remove);
    form->addRow(QString(), patternRow);

    m_useFolder = new QCheckBox(tr("Only files in this folder"), this);
    m_folder = new QLineEdit(group.folder, this);
    m_folder->setAccessibleName(tr("Folder"));
    m_browse = new QPushButton(tr("Browse..."), this);
    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(m_folder, 1);
    folderRow->addWidget(m_browse);
    form->addRow(QString(), m_useFolder);
    form->addRow(QString(), folderRow);
    m_useFolder->setChecked(!group.folder.isEmpty());
    auto syncFolder = [this]() {
        m_folder->setEnabled(m_useFolder->isChecked());
        m_browse->setEnabled(m_useFolder->isChecked());
    };
    connect(m_useFolder, &QCheckBox::toggled, this, syncFolder);
    syncFolder();

    m_tags = new QLineEdit(group.tags.join(QStringLiteral(", ")), this);
    m_tags->setAccessibleName(tr("Required tags"));
    m_tags->setPlaceholderText(tr("Files need all of these tags"));
    form->addRow(tr("T&ags"), m_tags);

    layout->addLayout(form);
    m_error = new QLabel(this);
    m_error->setWordWrap(true);
    m_error->setAccessibleName(tr("Error"));
    layout->addWidget(m_error);
    layout->addStretch(1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &GroupEditor::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(add, &QPushButton::clicked, this, &GroupEditor::addPattern);
    connect(m_patternEntry, &QLineEdit::returnPressed, this, &GroupEditor::addPattern);
    connect(fromFiles, &QPushButton::clicked, this, &GroupEditor::addFromFiles);
    connect(remove, &QPushButton::clicked, this, &GroupEditor::removePattern);
    connect(m_browse, &QPushButton::clicked, this, &GroupEditor::browseFolder);
}

void GroupEditor::addPattern()
{
    const QString text = m_patternEntry->text().trimmed();
    if (text.isEmpty()) {
        return;
    }
    if (!text.contains(QLatin1Char('/')) && text != QLatin1String("*")) {
        m_error->setText(tr("A file type looks like text/plain, or image/* for all images."));
        return;
    }
    m_error->clear();
    if (m_patterns->findItems(text, Qt::MatchExactly).isEmpty()) {
        m_patterns->addItem(text);
    }
    m_patternEntry->clear();
}

void GroupEditor::addFromFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Choose Example Files"));
    QMimeDatabase db;
    for (const QString &file : files) {
        const QString mime = db.mimeTypeForFile(file).name();
        if (m_patterns->findItems(mime, Qt::MatchExactly).isEmpty()) {
            m_patterns->addItem(mime);
        }
    }
}

void GroupEditor::removePattern()
{
    qDeleteAll(m_patterns->selectedItems());
}

void GroupEditor::browseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a Folder"), m_folder->text());
    if (!dir.isEmpty()) {
        m_folder->setText(dir);
    }
}

Group GroupEditor::group() const
{
    Group result = m_group;
    result.title = m_name->text().trimmed();
    result.iconNames = {m_icon->text().trimmed().isEmpty() ? QStringLiteral("folder") : m_icon->text().trimmed()};
    result.patterns.clear();
    for (int i = 0; i < m_patterns->count(); ++i) {
        result.patterns << m_patterns->item(i)->text();
    }
    result.folder = m_useFolder->isChecked() ? m_folder->text().trimmed() : QString();
    result.tags.clear();
    const QStringList tags = m_tags->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &tag : tags) {
        if (!tag.trimmed().isEmpty()) {
            result.tags << tag.trimmed();
        }
    }
    result.builtin = false;
    return result;
}

void GroupEditor::accept()
{
    const Group candidate = group();
    if (candidate.title.isEmpty()) {
        m_error->setText(tr("Give the group a name."));
        m_name->setFocus();
        return;
    }
    if (candidate.patterns.isEmpty() && candidate.folder.isEmpty() && candidate.tags.isEmpty()) {
        m_error->setText(tr("Add at least one file type, a folder or a tag."));
        return;
    }
    QDialog::accept();
}

} // namespace gui
} // namespace sheaf
