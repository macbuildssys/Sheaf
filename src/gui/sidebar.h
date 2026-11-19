#pragma once

#include <QListWidget>
#include <QStyledItemDelegate>

namespace sheaf {
namespace gui {

// Groups on top, then the tags. Each row can show a count
class Sidebar : public QListWidget {
    Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        CountRole,
        KindRole,
    };
    enum Kind {
        GroupKind = 1,
        TagKind,
        HeaderKind,
    };

    explicit Sidebar(QWidget *parent = nullptr);

    void addGroup(const QString &id, const QString &title, const QIcon &icon, int count);
    void addHeader(const QString &title);
    void addTag(const QString &name, int count);

    // Selects the row with this id and kind, returns false when it is not there
    bool selectEntry(Kind kind, const QString &id);

    Kind currentKind() const;
    QString currentId() const;

signals:
    void entryActivated(int kind, const QString &id);

private:
    void onCurrentChanged(QListWidgetItem *item);
};

class SidebarDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

} // namespace gui
} // namespace sheaf
