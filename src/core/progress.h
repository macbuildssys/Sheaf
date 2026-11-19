#pragma once

#include <QString>

namespace sheaf {

struct ProgressInfo {
    double fraction = -1.0;
    QString title;
    QString author;
};

// True when this build can read the metadata that document viewers keep
bool progressSupported();

// True for types where a viewer may have stored reading progress
bool progressCandidate(const QString &mime);

// Reads reading progress written by xreader and xviewer style apps through
// GVfs metadata. Returns an empty result when nothing is stored
ProgressInfo readReadingProgress(const QString &path);

// Interprets raw metadata strings, split out so it can be tested
ProgressInfo interpretProgress(const QString &page, const QString &pageCount, const QString &epubProgress,
                               const QString &epubTitle, const QString &epubAuthor);

} // namespace sheaf
