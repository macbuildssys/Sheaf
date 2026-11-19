#pragma once

#include <QString>
#include <QStringList>

namespace sheaf {
namespace pathutil {

// Returns the local path for a file URI, or an empty string for anything else
QString pathFromUri(const QString &uri);

// Builds a percent encoded file URI for a local path
QString uriFromPath(const QString &path);

// Makes a path absolute and removes dot segments, without touching the disk
QString cleanAbsolute(const QString &path);

// True when path equals dir or lies below it
bool isUnder(const QString &path, const QString &dir);

// Replaces a leading ~ with the home directory
QString expandHome(const QString &path);

// Mount points whose file system may block when it is unreachable
QStringList networkMountPoints(const QString &mountInfoFile = QStringLiteral("/proc/self/mountinfo"));

bool isOnAnyMount(const QString &path, const QStringList &mountPoints);

// Parses the text of a mountinfo file, split out so it can be tested
QStringList parseNetworkMounts(const QByteArray &mountInfo);

} // namespace pathutil
} // namespace sheaf
