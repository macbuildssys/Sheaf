#pragma once

#include "types.h"

#include <QList>
#include <QWidget>

class QLabel;
class QFormLayout;

namespace sheaf {
namespace gui {

class ThumbnailLoader;

// Shows what is known about the selected file. Actions live in the right click menu
class DetailsPanel : public QWidget {
    Q_OBJECT
public:
    DetailsPanel(ThumbnailLoader *loader, QWidget *parent = nullptr);

    void setDocuments(const QList<DocumentInfo> &docs);

protected:
    void changeEvent(QEvent *event) override;

private:
    void refresh();
    QLabel *addRow(const QString &caption);

    ThumbnailLoader *m_loader;
    QList<DocumentInfo> m_docs;
    QLabel *m_preview;
    QLabel *m_title;
    QFormLayout *m_form;
    QLabel *m_type;
    QLabel *m_size;
    QLabel *m_modified;
    QLabel *m_used;
    QLabel *m_opens;
    QLabel *m_location;
    QLabel *m_tags;
    QLabel *m_progress;
    QLabel *m_hint;
};

} // namespace gui
} // namespace sheaf
