#pragma once

#include "documentmodel.h"

#include <QWidget>

class QLineEdit;
class QListView;

namespace sheaf {
namespace gui {

class ThumbnailLoader;
class CardDelegate;

// A small search window for jumping to a document with the keyboard
class QuickOpen : public QWidget {
    Q_OBJECT
public:
    QuickOpen(DocumentModel *model, ThumbnailLoader *loader, QWidget *parent = nullptr);

    void popup();

signals:
    void openRequested(const sheaf::DocumentInfo &doc);
    void dismissed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void activateCurrent();

    DocumentFilterProxy *m_proxy;
    CardDelegate *m_delegate;
    QLineEdit *m_edit;
    QListView *m_list;
};

} // namespace gui
} // namespace sheaf
