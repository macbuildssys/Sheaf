#pragma once

#include "groups.h"
#include "types.h"

#include <QIcon>
#include <QPalette>
#include <QString>

namespace sheaf {
namespace gui {

QString relativeTime(const QDateTime &when);
QString humanSize(qint64 bytes);

// First icon that the current theme has, with a generic document as last resort
QIcon themeIcon(const QStringList &names);
QIcon groupIcon(const Group &group);

// Simple icons drawn in the current palette, so the toolbar looks the same on every icon theme
enum class Glyph {
    Grid,
    List,
    Details,
    Search,
    Sort,
};
QIcon glyphIcon(Glyph glyph, const QPalette &palette);

// One sentence that a screen reader reads for a document
QString accessibleSummary(const DocumentInfo &doc);

// Subtitle shown under a document title
QString subtitleFor(const DocumentInfo &doc);

} // namespace gui
} // namespace sheaf
