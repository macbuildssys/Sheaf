#pragma once

#include "settings.h"

#include <QDialog>

class QCheckBox;
class QListWidget;
class QSpinBox;

namespace sheaf {
namespace gui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(const Settings &settings, bool progressSupported, QWidget *parent = nullptr);

    // The settings with the choices made in the dialog applied
    Settings settings() const;

signals:
    void importThingyRequested();
    void exportRequested();
    void importRequested();
    void clearRecentsRequested();
    void removeMissingRequested();

private:
    void addExclusion();
    void removeExclusion();

    Settings m_settings;
    QCheckBox *m_confirmTrash;
    QCheckBox *m_generate;
    QCheckBox *m_includeKde;
    QSpinBox *m_maxItems;
    QListWidget *m_exclusions;
};

} // namespace gui
} // namespace sheaf
