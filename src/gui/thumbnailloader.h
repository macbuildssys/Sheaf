#pragma once

#include "thumbnails.h"
#include "types.h"

#include <QCache>
#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QThreadPool>

class QPalette;

namespace sheaf {
namespace gui {

// Hands out thumbnails without blocking. Missing ones are loaded on worker
// threads, the newest request first so that what is on screen wins
class ThumbnailLoader : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailLoader(QObject *parent = nullptr);
    ~ThumbnailLoader() override;

    void setGenerateEnabled(bool enabled) { m_generate = enabled; }

    // A ready thumbnail, or a null pixmap while it loads or when none exists
    QPixmap thumbnail(const DocumentInfo &doc);

    // Page shaped stand in with the file extension, coloured by document family
    static QPixmap placeholder(const DocumentInfo &doc, const QSize &logicalSize, qreal ratio, const QPalette &palette);

signals:
    void thumbnailReady();

private:
    void onLoaded(const QString &cacheKey, const QImage &image);

    QCache<QString, QPixmap> m_cache{400};
    QSet<QString> m_pending;
    QSet<QString> m_none;
    QThreadPool m_pool;
    int m_priority = 0;
    bool m_generate = true;
    bool m_notifyQueued = false;
};

} // namespace gui
} // namespace sheaf
