#include "sorting.h"

#include <QCollator>
#include <QLocale>

namespace sheaf {

namespace {

// Negative when a goes first in the default order of the mode
int compareDefault(const DocumentInfo &a, const DocumentInfo &b, SortMode mode)
{
    static const QCollator collator = [] {
        // The plain C locale compares bytes and ignores numbers, so fall back to English rules
        const QLocale system = QLocale::system();
        QCollator c(system.language() == QLocale::C ? QLocale(QLocale::English) : system);
        c.setCaseSensitivity(Qt::CaseInsensitive);
        c.setNumericMode(true);
        return c;
    }();
    switch (mode) {
    case SortMode::LastUsed:
        return a.lastUsed == b.lastUsed ? 0 : (a.lastUsed > b.lastUsed ? -1 : 1);
    case SortMode::Name:
        return collator.compare(a.displayName(), b.displayName());
    case SortMode::Modified:
        return a.modified == b.modified ? 0 : (a.modified > b.modified ? -1 : 1);
    case SortMode::Size:
        return a.size == b.size ? 0 : (a.size > b.size ? -1 : 1);
    case SortMode::Type: {
        const int byType = collator.compare(a.mimeComment, b.mimeComment);
        return byType != 0 ? byType : collator.compare(a.displayName(), b.displayName());
    }
    }
    return 0;
}

} // namespace

bool documentLess(const DocumentInfo &a, const DocumentInfo &b, const SortOptions &options)
{
    if (options.favoritesFirst && a.favorite != b.favorite) {
        return a.favorite;
    }
    int result = compareDefault(a, b, options.mode);
    if (options.reversed) {
        result = -result;
    }
    if (result != 0) {
        return result < 0;
    }
    // Fixed tie breakers keep the order stable between refreshes
    const int byName = QString::compare(a.name, b.name, Qt::CaseInsensitive);
    if (byName != 0) {
        return byName < 0;
    }
    return a.key < b.key;
}

} // namespace sheaf
