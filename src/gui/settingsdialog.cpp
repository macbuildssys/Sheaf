#include "settingsdialog.h"

#include "pathutil.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace sheaf {
namespace gui {

SettingsDialog::SettingsDialog(const Settings &settings, bool progressSupported, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Settings"));
    resize(560, 500);
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    layout->addWidget(tabs, 1);

    auto *general = new QWidget(this);
    auto *generalLayout = new QVBoxLayout(general);
    m_confirmTrash = new QCheckBox(tr("Ask before moving files to the trash"), general);
    m_confirmTrash->setChecked(settings.confirmTrash);
    m_generate = new QCheckBox(tr("Create thumbnails for files that have none"), general);
    m_generate->setChecked(settings.generateThumbnails);
    m_generate->setToolTip(tr("Uses the thumbnail programs installed on this system. Thumbnails are stored in the shared thumbnail cache."));
    generalLayout->addWidget(m_confirmTrash);
    generalLayout->addWidget(m_generate);
    if (!progressSupported) {
        auto *note = new QLabel(tr("This build cannot read reading progress from document viewers."), general);
        note->setWordWrap(true);
        generalLayout->addWidget(note);
    }
    generalLayout->addStretch(1);
    tabs->addTab(general, tr("General"));

    auto *library = new QWidget(this);
    auto *libraryLayout = new QVBoxLayout(library);
    m_includeKde = new QCheckBox(tr("Include the recent files list of KDE applications"), library);
    m_includeKde->setChecked(settings.includeKde);
    auto *limitRow = new QHBoxLayout;
    auto *limitLabel = new QLabel(tr("Keep at most this many recent files:"), library);
    m_maxItems = new QSpinBox(library);
    m_maxItems->setRange(50, 100000);
    m_maxItems->setSingleStep(500);
    m_maxItems->setValue(settings.maxItems);
    limitLabel->setBuddy(m_maxItems);
    limitRow->addWidget(limitLabel);
    limitRow->addWidget(m_maxItems);
    limitRow->addStretch(1);
    libraryLayout->addWidget(m_includeKde);
    libraryLayout->addLayout(limitRow);
    auto *excludeLabel = new QLabel(tr("Files in these folders are hidden, unless they are favorites:"), library);
    excludeLabel->setWordWrap(true);
    libraryLayout->addWidget(excludeLabel);
    m_exclusions = new QListWidget(library);
    m_exclusions->setAccessibleName(tr("Hidden folders"));
    m_exclusions->addItems(settings.exclusions);
    excludeLabel->setBuddy(m_exclusions);
    libraryLayout->addWidget(m_exclusions, 1);
    auto *excludeButtons = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add Folder..."), library);
    auto *remove = new QPushButton(tr("Remove"), library);
    excludeButtons->addWidget(add);
    excludeButtons->addWidget(remove);
    excludeButtons->addStretch(1);
    libraryLayout->addLayout(excludeButtons);
    tabs->addTab(library, tr("Library"));

    auto *dataPage = new QWidget(this);
    auto *dataLayout = new QVBoxLayout(dataPage);
    auto addAction = [&](const QString &text, const QString &hint, auto signal) {
        auto *button = new QPushButton(text, dataPage);
        auto *label = new QLabel(hint, dataPage);
        label->setWordWrap(true);
        QPalette pal = label->palette();
        QColor dim = pal.color(QPalette::WindowText);
        dim.setAlpha(170);
        pal.setColor(QPalette::WindowText, dim);
        label->setPalette(pal);
        dataLayout->addWidget(button, 0, Qt::AlignLeft);
        dataLayout->addWidget(label);
        dataLayout->addSpacing(8);
        connect(button, &QPushButton::clicked, this, signal);
    };
    addAction(tr("Import from Thingy..."), tr("Copies your favorites and custom groups from Linux Mint's Thingy."), [this]() { emit importThingyRequested(); });
    addAction(tr("Export Library..."), tr("Saves your favorites and tags to a file."), [this]() { emit exportRequested(); });
    addAction(tr("Import Library..."), tr("Restores favorites and tags from a file made by Export."), [this]() { emit importRequested(); });
    addAction(tr("Remove Missing Files"), tr("Forgets files that were moved or deleted."), [this]() { emit removeMissingRequested(); });
    addAction(tr("Clear Recent Files..."), tr("Clears the recent files list for all applications."), [this]() { emit clearRecentsRequested(); });
    dataLayout->addStretch(1);
    tabs->addTab(dataPage, tr("Data"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(add, &QPushButton::clicked, this, &SettingsDialog::addExclusion);
    connect(remove, &QPushButton::clicked, this, &SettingsDialog::removeExclusion);
}

void SettingsDialog::addExclusion()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a Folder to Hide"), pathutil::expandHome(QStringLiteral("~")));
    if (!dir.isEmpty() && m_exclusions->findItems(dir, Qt::MatchExactly).isEmpty()) {
        m_exclusions->addItem(dir);
    }
}

void SettingsDialog::removeExclusion()
{
    qDeleteAll(m_exclusions->selectedItems());
}

Settings SettingsDialog::settings() const
{
    Settings result = m_settings;
    result.confirmTrash = m_confirmTrash->isChecked();
    result.generateThumbnails = m_generate->isChecked();
    result.includeKde = m_includeKde->isChecked();
    result.showMissing = false;
    result.maxItems = m_maxItems->value();
    result.exclusions.clear();
    for (int i = 0; i < m_exclusions->count(); ++i) {
        result.exclusions << m_exclusions->item(i)->text();
    }
    return result;
}

} // namespace gui
} // namespace sheaf
