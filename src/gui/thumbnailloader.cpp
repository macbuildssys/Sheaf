#include "thumbnailloader.h"

#include "groups.h"
#include "pathutil.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QTimer>

namespace sheaf {
namespace gui {

ThumbnailLoader::ThumbnailLoader(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(3);
}

ThumbnailLoader::~ThumbnailLoader()
{
    m_pool.clear();
    m_pool.waitForDone();
}

static QString cacheKeyFor(const DocumentInfo &doc)
{
    return doc.key + QLatin1Char('|') + QString::number(doc.modified.toSecsSinceEpoch());
}

QPixmap ThumbnailLoader::thumbnail(const DocumentInfo &doc)
{
    if (!doc.exists) {
        return QPixmap();
    }
    const QString key = cacheKeyFor(doc);
    if (const QPixmap *hit = m_cache.object(key)) {
        return *hit;
    }
    if (m_pending.contains(key) || m_none.contains(key)) {
        return QPixmap();
    }
    m_pending.insert(key);

    ThumbRequest request;
    request.path = doc.path;
    request.uri = doc.uri;
    request.mime = doc.mime;
    request.mtime = doc.modified.toSecsSinceEpoch();
    request.allowGenerate = m_generate && !doc.remote;
    m_pool.start(
        [this, request, key]() {
            const QImage image = thumbnails::load(request);
            QMetaObject::invokeMethod(this, [this, key, image]() { onLoaded(key, image); }, Qt::QueuedConnection);
        },
        ++m_priority);
    return QPixmap();
}

void ThumbnailLoader::onLoaded(const QString &cacheKey, const QImage &image)
{
    m_pending.remove(cacheKey);
    if (image.isNull()) {
        m_none.insert(cacheKey);
    } else {
        m_cache.insert(cacheKey, new QPixmap(QPixmap::fromImage(image)));
    }
    // Many thumbnails arrive together, so repaint once for the batch
    if (!m_notifyQueued) {
        m_notifyQueued = true;
        QTimer::singleShot(60, this, [this]() {
            m_notifyQueued = false;
            emit thumbnailReady();
        });
    }
}

QPixmap ThumbnailLoader::placeholder(const DocumentInfo &doc, const QSize &logicalSize, qreal ratio, const QPalette &palette)
{
    static const QColor colors[] = {
        QColor(0xB0, 0x3A, 0x2E), QColor(0x2E, 0x5C, 0x9E), QColor(0x2E, 0x7D, 0x4F), QColor(0xA8, 0x55, 0x10), QColor(0x7B, 0x4F, 0xA3),
        QColor(0xA2, 0x3B, 0x72), QColor(0x3D, 0x4C, 0xA8), QColor(0x8A, 0x5A, 0x2B), QColor(0x52, 0x60, 0x6D), QColor(0x5F, 0x63, 0x68),
    };
    const QSize pixelSize = logicalSize * ratio;
    QPixmap pix(pixelSize);
    pix.setDevicePixelRatio(ratio);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF page(0.5, 0.5, logicalSize.width() - 1.0, logicalSize.height() - 1.0);
    const qreal fold = qMin(page.width(), page.height()) * 0.22;
    const QColor accent = colors[familyForMime(doc.mime)];

    QPainterPath shape;
    shape.moveTo(page.topLeft());
    shape.lineTo(page.right() - fold, page.top());
    shape.lineTo(page.right(), page.top() + fold);
    shape.lineTo(page.bottomRight());
    shape.lineTo(page.bottomLeft());
    shape.closeSubpath();

    p.setPen(QPen(palette.color(QPalette::Mid), 1));
    p.setBrush(palette.color(QPalette::Base));
    p.drawPath(shape);

    QPainterPath corner;
    corner.moveTo(page.right() - fold, page.top());
    corner.lineTo(page.right() - fold, page.top() + fold);
    corner.lineTo(page.right(), page.top() + fold);
    corner.closeSubpath();
    p.setBrush(palette.color(QPalette::AlternateBase));
    p.drawPath(corner);

    const QRectF band(page.left() + page.width() * 0.14, page.top() + page.height() * 0.36, page.width() * 0.72, page.height() * 0.28);
    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    p.drawRoundedRect(band, 4, 4);

    QString label = doc.extension.isEmpty() ? QStringLiteral("?") : doc.extension.left(5).toUpper();
    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(qMax(10, int(band.height() * 0.5)));
    QFontMetrics fm(font);
    while (fm.horizontalAdvance(label) > band.width() * 0.9 && font.pixelSize() > 8) {
        font.setPixelSize(font.pixelSize() - 1);
        fm = QFontMetrics(font);
    }
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(band, Qt::AlignCenter, label);

    // Text lines hint at a page of content
    p.setPen(QPen(palette.color(QPalette::Mid), qMax(1.0, page.height() * 0.018), Qt::SolidLine, Qt::RoundCap));
    for (int i = 0; i < 3; ++i) {
        const qreal y = page.top() + page.height() * (0.72 + i * 0.07);
        const qreal w = page.width() * (i == 2 ? 0.34 : 0.62);
        p.drawLine(QPointF(page.left() + page.width() * 0.14, y), QPointF(page.left() + page.width() * 0.14 + w, y));
    }
    p.end();
    return pix;
}

} // namespace gui
} // namespace sheaf
