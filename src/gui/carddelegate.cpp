#include "carddelegate.h"

#include "documentmodel.h"
#include "guiutil.h"
#include "thumbnailloader.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace sheaf {
namespace gui {

namespace {

constexpr int kPad = 10;

// Wraps text onto at most two lines and elides what does not fit
QStringList twoLines(const QString &text, const QFontMetrics &fm, int width)
{
    QStringList lines;
    QString rest = text;
    for (int i = 0; i < 2 && !rest.isEmpty(); ++i) {
        if (i == 1 || fm.horizontalAdvance(rest) <= width) {
            lines << fm.elidedText(rest, Qt::ElideMiddle, width);
            rest.clear();
            break;
        }
        // Break at the last space that fits, otherwise anywhere
        int cut = 0;
        int lastSpace = -1;
        for (int c = 1; c <= rest.size(); ++c) {
            if (fm.horizontalAdvance(rest.left(c)) > width) {
                break;
            }
            cut = c;
            if (rest.at(c - 1).isSpace() || rest.at(c - 1) == QLatin1Char('_') || rest.at(c - 1) == QLatin1Char('-')) {
                lastSpace = c;
            }
        }
        if (cut == 0) {
            cut = 1;
        }
        if (lastSpace > cut / 3) {
            cut = lastSpace;
        }
        lines << rest.left(cut).trimmed();
        rest = rest.mid(cut).trimmed();
    }
    return lines;
}

} // namespace

CardDelegate::CardDelegate(ThumbnailLoader *loader, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_loader(loader)
{
}

void CardDelegate::setMode(Mode mode)
{
    m_mode = mode;
}

void CardDelegate::setZoom(int zoom)
{
    m_zoom = zoom;
}

QSize CardDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    const QFontMetrics fm(option.font);
    switch (m_mode) {
    case Mode::Grid:
        return QSize(m_zoom + 2 * kPad + 8, static_cast<int>(m_zoom * 1.25) + fm.height() * 3 + 3 * kPad);
    case Mode::List:
        return QSize(200, qMax(56, fm.height() * 2 + 2 * kPad));
    case Mode::Compact:
        return QSize(200, qMax(40, fm.height() * 2 + kPad));
    }
    return QSize();
}

void CardDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const DocumentInfo doc = index.data(DocumentModel::DocumentRole).value<DocumentInfo>();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    if (m_mode == Mode::Grid) {
        paintGrid(painter, option, doc);
    } else {
        paintRow(painter, option, doc);
    }
    painter->restore();
}

void CardDelegate::paintBackground(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect)
{
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(option.palette.color(QPalette::Highlight));
        painter->drawRoundedRect(rect, 8, 8);
    } else if (hovered) {
        QColor tint = option.palette.color(QPalette::Text);
        tint.setAlpha(22);
        painter->setPen(Qt::NoPen);
        painter->setBrush(tint);
        painter->drawRoundedRect(rect, 8, 8);
    }
    // A focus ring that never depends on colour alone, for keyboard users
    if ((option.state & QStyle::State_HasFocus) && (option.state & QStyle::State_KeyboardFocusChange || selected || hovered ||
                                                    option.state & QStyle::State_Active)) {
        const QColor ring = selected ? option.palette.color(QPalette::HighlightedText) : option.palette.color(QPalette::Highlight);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(ring, 2));
        painter->drawRoundedRect(rect.adjusted(1, 1, -1, -1), 7, 7);
    }
}

QRect CardDelegate::starRect(const QStyleOptionViewItem &option) const
{
    constexpr int kStar = 20;
    if (m_mode == Mode::Grid) {
        const QRect cell = option.rect.adjusted(3, 3, -3, -3);
        return QRect(cell.right() - kPad - kStar + 4, cell.top() + kPad - 4, kStar, kStar);
    }
    const QRect row = option.rect.adjusted(2, 1, -2, -1);
    return QRect(row.right() - kPad - kStar, row.center().y() - kStar / 2, kStar, kStar);
}

void CardDelegate::paintFavoriteMarker(QPainter *painter, const QStyleOptionViewItem &option, const DocumentInfo &doc) const
{
    const bool hovered = option.state & QStyle::State_MouseOver;
    if (!doc.favorite && !hovered) {
        return;
    }
    paintStar(painter, QRectF(starRect(option)).adjusted(1, 1, -1, -1), doc.favorite);
}

void CardDelegate::paintStar(QPainter *painter, const QRectF &box, bool filled)
{
    QPainterPath star;
    const QPointF center = box.center();
    const qreal outer = box.width() / 2.0;
    const qreal inner = outer * 0.45;
    for (int i = 0; i < 10; ++i) {
        const qreal radius = (i % 2 == 0) ? outer : inner;
        const qreal angle = -M_PI / 2 + i * M_PI / 5;
        const QPointF point(center.x() + radius * std::cos(angle), center.y() + radius * std::sin(angle));
        if (i == 0) {
            star.moveTo(point);
        } else {
            star.lineTo(point);
        }
    }
    star.closeSubpath();
    if (filled) {
        // A dark outline keeps the star visible on light and dark thumbnails
        painter->setPen(QPen(QColor(0x40, 0x30, 0x00), 1.2));
        painter->setBrush(QColor(0xFF, 0xC1, 0x07));
    } else {
        // Hollow star as a hint that a click makes this a favorite
        painter->setPen(QPen(QColor(0x80, 0x80, 0x80, 230), 1.4));
        painter->setBrush(QColor(255, 255, 255, 150));
    }
    painter->drawPath(star);
}

void CardDelegate::paintThumbnail(QPainter *painter, const QStyleOptionViewItem &option, const DocumentInfo &doc, const QRect &box) const
{
    const qreal ratio = painter->device()->devicePixelRatioF();
    QPixmap pix = m_loader->thumbnail(doc);
    QRect target;
    if (pix.isNull()) {
        const QSize size(qMax(24, static_cast<int>(box.width() * 0.78)), qMax(30, static_cast<int>(box.height() * 0.95)));
        const QSize fitted = QSize(size.width(), static_cast<int>(size.width() * 1.3)).boundedTo(box.size()).boundedTo(size);
        pix = ThumbnailLoader::placeholder(doc, QSize(fitted.width(), qMin(box.height(), static_cast<int>(fitted.width() * 1.3))), ratio,
                                           option.palette);
        const QSize logical = pix.size() / pix.devicePixelRatio();
        target = QRect(QPoint(box.center().x() - logical.width() / 2, box.bottom() - logical.height()), logical);
    } else {
        QSize logical = pix.size();
        logical.scale(box.size(), Qt::KeepAspectRatio);
        target = QRect(QPoint(box.center().x() - logical.width() / 2, box.bottom() - logical.height()), logical);
        painter->setPen(QPen(option.palette.color(QPalette::Mid), 1));
        painter->setBrush(option.palette.color(QPalette::Base));
        painter->drawRoundedRect(target.adjusted(-1, -1, 1, 1), 3, 3);
    }
    painter->setOpacity(doc.exists ? 1.0 : 0.45);
    painter->drawPixmap(target, pix);
    painter->setOpacity(1.0);

    if (doc.progress >= 0.0) {
        const QRect track(target.left() + 6, target.bottom() - 10, target.width() - 12, 5);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 120));
        painter->drawRoundedRect(track, 2.5, 2.5);
        painter->setBrush(option.palette.color(QPalette::Highlight).lighter(130));
        painter->drawRoundedRect(QRect(track.left(), track.top(), static_cast<int>(track.width() * doc.progress), track.height()), 2.5, 2.5);
    }
}

void CardDelegate::paintGrid(QPainter *painter, const QStyleOptionViewItem &option, const DocumentInfo &doc) const
{
    const QRect cell = option.rect.adjusted(3, 3, -3, -3);
    paintBackground(painter, option, cell);

    const bool selected = option.state & QStyle::State_Selected;
    const QColor text = option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
    QColor dim = text;
    dim.setAlpha(selected ? 210 : 160);

    const QFontMetrics fm(option.font);
    const QRect inner = cell.adjusted(kPad, kPad, -kPad, -kPad);
    const int thumbHeight = static_cast<int>(m_zoom * 1.25);
    const QRect thumbBox(inner.left(), inner.top(), inner.width(), thumbHeight);
    paintThumbnail(painter, option, doc, thumbBox);
    paintFavoriteMarker(painter, option, doc);

    int y = thumbBox.bottom() + 8;
    painter->setPen(text);
    const QStringList lines = twoLines(doc.displayName(), fm, inner.width());
    for (const QString &line : lines) {
        painter->drawText(QRect(inner.left(), y, inner.width(), fm.height()), Qt::AlignHCenter | Qt::AlignVCenter, line);
        y += fm.height();
    }
    y = thumbBox.bottom() + 8 + fm.height() * 2;
    QFont small = option.font;
    small.setPointSizeF(qMax(7.0, option.font.pointSizeF() * 0.88));
    painter->setFont(small);
    painter->setPen(doc.exists ? dim : text);
    const QFontMetrics sfm(small);
    painter->drawText(QRect(inner.left(), y, inner.width(), sfm.height() + 2), Qt::AlignHCenter | Qt::AlignVCenter,
                      sfm.elidedText(subtitleFor(doc), Qt::ElideRight, inner.width()));
}

void CardDelegate::paintRow(QPainter *painter, const QStyleOptionViewItem &option, const DocumentInfo &doc) const
{
    const QRect row = option.rect.adjusted(2, 1, -2, -1);
    paintBackground(painter, option, row);

    const bool selected = option.state & QStyle::State_Selected;
    const QColor text = option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
    QColor dim = text;
    dim.setAlpha(selected ? 210 : 160);

    const QFontMetrics fm(option.font);
    const int thumbSize = row.height() - 2 * 4;
    const QRect thumbBox(row.left() + kPad, row.top() + 4, thumbSize, thumbSize);
    paintThumbnail(painter, option, doc, thumbBox);

    // Right hand columns: modified date and size, hidden when the row is narrow
    const int starSpace = 28;
    int right = row.right() - kPad - starSpace;
    const bool wide = row.width() > 560 && m_mode == Mode::List;
    if (wide) {
        const int sizeWidth = fm.horizontalAdvance(QStringLiteral("000.0 MiB")) + 8;
        const int dateWidth = fm.horizontalAdvance(QStringLiteral("Yesterday 00:00 AM")) + 8;
        painter->setPen(dim);
        painter->drawText(QRect(right - sizeWidth, row.top(), sizeWidth, row.height()), Qt::AlignRight | Qt::AlignVCenter,
                          doc.exists ? humanSize(doc.size) : QString());
        right -= sizeWidth + 8;
        painter->drawText(QRect(right - dateWidth, row.top(), dateWidth, row.height()), Qt::AlignRight | Qt::AlignVCenter,
                          relativeTime(doc.modified));
        right -= dateWidth + 8;
    }
    paintFavoriteMarker(painter, option, doc);

    const int textLeft = thumbBox.right() + 12;
    const int textWidth = qMax(40, right - textLeft);
    painter->setPen(text);
    QFont title = option.font;
    painter->setFont(title);
    const int blockHeight = fm.height() * 2;
    const int top = row.center().y() - blockHeight / 2;
    painter->drawText(QRect(textLeft, top, textWidth, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(doc.displayName(), Qt::ElideMiddle, textWidth));
    painter->setPen(doc.exists ? dim : text);
    const QString second = doc.exists ? (m_mode == Mode::List ? doc.folder() : subtitleFor(doc)) : QCoreApplication::translate("Format", "Missing") + QStringLiteral("  ") + doc.folder();
    painter->drawText(QRect(textLeft, top + fm.height(), textWidth, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(second, Qt::ElideMiddle, textWidth));
}

} // namespace gui
} // namespace sheaf
