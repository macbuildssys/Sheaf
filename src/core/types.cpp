#include "types.h"

#include <QFileInfo>

namespace sheaf {

QString DocumentInfo::folder() const
{
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0) {
        return QStringLiteral("/");
    }
    return path.left(slash);
}

QString sortModeToString(SortMode mode)
{
    switch (mode) {
    case SortMode::LastUsed:
        return QStringLiteral("used");
    case SortMode::Name:
        return QStringLiteral("name");
    case SortMode::Modified:
        return QStringLiteral("modified");
    case SortMode::Size:
        return QStringLiteral("size");
    case SortMode::Type:
        return QStringLiteral("type");
    }
    return QStringLiteral("used");
}

SortMode sortModeFromString(const QString &text, bool *ok)
{
    static const struct {
        const char *name;
        SortMode mode;
    } table[] = {
        {"used", SortMode::LastUsed}, {"name", SortMode::Name}, {"modified", SortMode::Modified},
        {"size", SortMode::Size},     {"type", SortMode::Type},
    };
    for (const auto &entry : table) {
        if (text == QLatin1String(entry.name)) {
            if (ok) {
                *ok = true;
            }
            return entry.mode;
        }
    }
    if (ok) {
        *ok = false;
    }
    return SortMode::LastUsed;
}

} // namespace sheaf
