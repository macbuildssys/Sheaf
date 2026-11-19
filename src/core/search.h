#pragma once

#include "types.h"

#include <QString>
#include <QStringList>

namespace sheaf {

// Case and accent insensitive form used for all text comparisons
QString foldForSearch(const QString &text);

// Free text plus a few operators:
//   tag:name  type:pdf  in:folder  is:fav  is:missing  and -word to exclude.
// Words in double quotes are matched as one phrase
struct SearchQuery {
    QStringList words;
    QStringList excluded;
    QStringList tags;
    QStringList types;
    QStringList folders;
    bool onlyFavorites = false;
    bool onlyMissing = false;

    bool isEmpty() const;
    bool matches(const DocumentInfo &doc) const;

    static SearchQuery parse(const QString &text);
    static QString buildHaystack(const DocumentInfo &doc);
};

} // namespace sheaf
