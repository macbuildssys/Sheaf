#include "sidebar.h"

#include <QPainter>

namespace sheaf {
namespace gui {

Sidebar::Sidebar(QWidget *parent)
    : QListWidget(parent)
{
    setItemDelegate(new SidebarDelegate(this));
    setIconSize(QSize(20, 20));
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setAccessibleName(tr("Groups and tags"));
    connect(this, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item, QListWidgetItem *) { onCurrentChanged(item); });
}

void Sidebar::addGroup(const QString &id, const QString &title, const QIcon &icon, int count)
{
    auto *item = new QListWidgetItem(icon, title, this);
    item->setData(IdRole, id);
    item->setData(CountRole, count);
    item->setData(KindRole, GroupKind);
    item->setData(Qt::AccessibleTextRole, tr("%1, %n file(s)", nullptr, count).arg(title));
}

void Sidebar::addHeader(const QString &title)
{
    auto *item = new QListWidgetItem(title, this);
    item->setData(KindRole, HeaderKind);
    item->setFlags(Qt::ItemIsEnabled);
}

void Sidebar::addTag(const QString &name, int count)
{
    auto *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("tag"), QIcon::fromTheme(QStringLiteral("bookmark-new"))), name, this);
    item->setData(IdRole, name);
    item->setData(CountRole, count);
    item->setData(KindRole, TagKind);
    item->setData(Qt::AccessibleTextRole, tr("Tag %1, %n file(s)", nullptr, count).arg(name));
}

bool Sidebar::selectEntry(Kind kind, const QString &id)
{
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *entry = item(i);
        if (entry->data(KindRole).toInt() == kind && entry->data(IdRole).toString() == id) {
            setCurrentItem(entry);
            return true;
        }
    }
    return false;
}

Sidebar::Kind Sidebar::currentKind() const
{
    const QListWidgetItem *entry = currentItem();
    return entry ? static_cast<Kind>(entry->data(KindRole).toInt()) : GroupKind;
}

QString Sidebar::currentId() const
{
    const QListWidgetItem *entry = currentItem();
    return entry ? entry->data(IdRole).toString() : QString();
}

void Sidebar::onCurrentChanged(QListWidgetItem *entry)
{
    if (!entry || entry->data(KindRole).toInt() == HeaderKind) {
        return;
    }
    emit entryActivated(entry->data(KindRole).toInt(), entry->data(IdRole).toString());
}

QSize SidebarDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height() + 10, 34));
    return size;
}

void SidebarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.data(Sidebar::KindRole).toInt() == Sidebar::HeaderKind) {
        painter->save();
        QFont font = option.font;
        font.setBold(true);
        font.setPointSizeF(qMax(7.0, font.pointSizeF() * 0.85));
        painter->setFont(font);
        QColor color = option.palette.color(QPalette::Text);
        color.setAlpha(170);
        painter->setPen(color);
        painter->drawText(option.rect.adjusted(14, 8, -8, 0), Qt::AlignLeft | Qt::AlignVCenter, index.data(Qt::DisplayRole).toString().toUpper());
        painter->restore();
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRect row = option.rect.adjusted(6, 2, -6, -2);
    const bool selected = option.state & QStyle::State_Selected;
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(option.palette.color(QPalette::Highlight));
        painter->drawRoundedRect(row, 7, 7);
    } else if (option.state & QStyle::State_MouseOver) {
        QColor tint = option.palette.color(QPalette::Text);
        tint.setAlpha(22);
        painter->setPen(Qt::NoPen);
        painter->setBrush(tint);
        painter->drawRoundedRect(row, 7, 7);
    }
    if ((option.state & QStyle::State_HasFocus) && (option.state & QStyle::State_KeyboardFocusChange)) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(selected ? option.palette.color(QPalette::HighlightedText) : option.palette.color(QPalette::Highlight), 2));
        painter->drawRoundedRect(row.adjusted(1, 1, -1, -1), 6, 6);
    }

    const QColor text = option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    int left = row.left() + 10;
    if (!icon.isNull()) {
        const QSize size = option.decorationSize;
        const QRect iconRect(left, row.center().y() - size.height() / 2, size.width(), size.height());
        icon.paint(painter, iconRect, Qt::AlignCenter, selected ? QIcon::Selected : QIcon::Normal);
        left += size.width() + 10;
    }
    const int count = index.data(Sidebar::CountRole).toInt();
    const QString countText = QString::number(count);
    const QFontMetrics fm(option.font);
    const int countWidth = fm.horizontalAdvance(countText) + 4;
    painter->setPen(text);
    painter->drawText(QRect(left, row.top(), row.right() - left - countWidth - 12, row.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, row.right() - left - countWidth - 12));
    QColor dim = text;
    dim.setAlpha(selected ? 220 : 150);
    painter->setPen(dim);
    painter->drawText(QRect(row.right() - countWidth - 8, row.top(), countWidth, row.height()), Qt::AlignRight | Qt::AlignVCenter, countText);
    painter->restore();
}

} // namespace gui
} // namespace sheaf
