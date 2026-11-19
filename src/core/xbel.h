#pragma once

#include <QDateTime>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace sheaf {

struct RecentEntry {
    QString uri;
    QString mime;
    QDateTime added;
    QDateTime modified;
    QDateTime visited;
    QStringList apps;
    bool isPrivate = false;
};

struct XbelResult {
    QVector<RecentEntry> entries;
    QString error;
    bool truncated = false;
};

// Reader and editor for the freedesktop recently-used.xbel file
namespace xbel {

constexpr qint64 kMaxFileBytes = 64 * 1024 * 1024;
constexpr int kMaxEntries = 200000;

XbelResult readFile(const QString &path);
XbelResult readData(const QByteArray &data, int maxEntries = kMaxEntries);

// Removes every bookmark that points to one of the given local paths
bool removePaths(const QString &file, const QSet<QString> &paths, int *removed, QString *error);

// Replaces the file with an empty list
bool clear(const QString &file, QString *error);

QByteArray emptyDocument();

} // namespace xbel
} // namespace sheaf
