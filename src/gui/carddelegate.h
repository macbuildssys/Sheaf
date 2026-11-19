#pragma once

#include "types.h"

#include <QStyledItemDelegate>

namespace sheaf {
namespace gui {

class ThumbnailLoader;

class CardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    enum class Mode {
        Grid,
        List,
        Compact,
    };

    CardDelegate(ThumbnailLoader *loader, QObject *parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }
    void setZoom(int zoom);
    int zoom() const { return m_zoom; }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Where the favorite star sits inside an item, so a click on it can be recognised
    QRect starRect(const QStyleOptionViewItem &option) const;

private:
    void paintGrid(QPainter *painter, const QStyleOptionViewItem &option, const DocumentInfo &doc) const;
    void paintRow(QPainter *painter, const QStyleOptionViewItem &option, const DocumentInfo &doc) const;
    void paintThumbnail(QPainter *painter, const QStyleOptionViewItem &option, const DocumentInfo &doc, const QRect &box) const;
    static void paintStar(QPainter *painter, const QRectF &box, bool filled);
    void paintFavoriteMarker(QPainter *painter, const QStyleOptionViewItem &option, const DocumentInfo &doc) const;
    static void paintBackground(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect);

    ThumbnailLoader *m_loader;
    Mode m_mode = Mode::Grid;
    int m_zoom = 160;
};

} // namespace gui
} // namespace sheaf
