#include "guiutil.h"

#include <QPainter>
#include <QPixmap>

#include <QCoreApplication>
#include <QLocale>

namespace sheaf {
namespace gui {

QString relativeTime(const QDateTime &when)
{
    if (!when.isValid()) {
        return QString();
    }
    const QDateTime local = when.toLocalTime();
    const QDate today = QDate::currentDate();
    const int days = local.date().daysTo(today);
    const QLocale locale;
    if (days == 0) {
        return QCoreApplication::translate("Format", "Today %1").arg(locale.toString(local.time(), QLocale::ShortFormat));
    }
    if (days == 1) {
        return QCoreApplication::translate("Format", "Yesterday %1").arg(locale.toString(local.time(), QLocale::ShortFormat));
    }
    if (days > 1 && days < 7) {
        return QCoreApplication::translate("Format", "%n days ago", nullptr, days);
    }
    return locale.toString(local.date(), QLocale::ShortFormat);
}

QString humanSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

QIcon themeIcon(const QStringList &names)
{
    for (const QString &name : names) {
        if (QIcon::hasThemeIcon(name)) {
            return QIcon::fromTheme(name);
        }
    }
    const QIcon generic = QIcon::fromTheme(QStringLiteral("text-x-generic"));
    return generic.isNull() ? QIcon::fromTheme(QStringLiteral("unknown")) : generic;
}

QIcon groupIcon(const Group &group)
{
    return themeIcon(group.iconNames);
}

QString subtitleFor(const DocumentInfo &doc)
{
    if (!doc.exists) {
        return QCoreApplication::translate("Format", "Missing");
    }
    QStringList parts;
    const QString when = relativeTime(doc.lastUsed);
    if (!when.isEmpty()) {
        parts << when;
    }
    if (!doc.mimeComment.isEmpty()) {
        parts << doc.mimeComment;
    }
    return parts.join(QStringLiteral("  "));
}

QString accessibleSummary(const DocumentInfo &doc)
{
    QStringList parts;
    parts << doc.displayName();
    if (!doc.mimeComment.isEmpty()) {
        parts << doc.mimeComment;
    }
    if (doc.favorite) {
        parts << QCoreApplication::translate("Format", "Favorite");
    }
    if (!doc.exists) {
        parts << QCoreApplication::translate("Format", "File is missing");
    }
    if (doc.progress >= 0.0) {
        parts << QCoreApplication::translate("Format", "%1 percent read").arg(qRound(doc.progress * 100));
    }
    if (!doc.tags.isEmpty()) {
        parts << QCoreApplication::translate("Format", "Tags: %1").arg(doc.tags.join(QStringLiteral(", ")));
    }
    const QString when = relativeTime(doc.lastUsed);
    if (!when.isEmpty()) {
        parts << when;
    }
    return parts.join(QStringLiteral(", "));
}

QIcon glyphIcon(Glyph glyph, const QPalette &palette)
{
    constexpr int kSize = 22;
    constexpr qreal kRatio = 2.0;
    QIcon icon;
    for (const QIcon::Mode mode : {QIcon::Normal, QIcon::Disabled}) {
        const QColor color = palette.color(mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active, QPalette::ButtonText);
        QPixmap pix(static_cast<int>(kSize * kRatio), static_cast<int>(kSize * kRatio));
        pix.fill(Qt::transparent);
        pix.setDevicePixelRatio(kRatio);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        switch (glyph) {
        case Glyph::Grid:
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 2; ++col) {
                    p.drawRoundedRect(QRectF(3 + col * 8.5, 3 + row * 8.5, 7.5, 7.5), 2, 2);
                }
            }
            break;
        case Glyph::List:
            for (int row = 0; row < 3; ++row) {
                const qreal y = 4 + row * 6;
                p.drawRoundedRect(QRectF(3, y, 3, 3), 1, 1);
                p.drawRoundedRect(QRectF(8, y, 11, 3), 1.5, 1.5);
            }
            break;
        case Glyph::Details:
            p.setPen(QPen(color, 1.6));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(3, 4, 16, 14), 2, 2);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRect(QRectF(13, 5, 5.2, 12));
            break;
        case Glyph::Search:
            p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(9.5, 9.5), 5.5, 5.5);
            p.drawLine(QPointF(13.6, 13.6), QPointF(18.5, 18.5));
            break;
        case Glyph::Sort:
            p.drawRoundedRect(QRectF(3, 4, 16, 3), 1.5, 1.5);
            p.drawRoundedRect(QRectF(3, 9.5, 11, 3), 1.5, 1.5);
            p.drawRoundedRect(QRectF(3, 15, 6, 3), 1.5, 1.5);
            break;
        }
        p.end();
        icon.addPixmap(pix, mode);
    }
    return icon;
}

} // namespace gui
} // namespace sheaf
