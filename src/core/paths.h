#pragma once

#include <QString>

namespace sheaf {

// All locations follow the XDG base directory rules, so tests can redirect
// everything by setting XDG_DATA_HOME, XDG_CONFIG_HOME and XDG_CACHE_HOME
namespace paths {

QString dataDir();
QString configDir();
QString databaseFile();
QString groupsFile();
QString settingsFile();
QString recentFile();
QString kdeRecentDir();
QString thumbnailRoot();
QString trashFilesDir();

// Creates a directory that only the current user can enter
bool ensurePrivateDir(const QString &path);

// Restricts a file to the current user
void makePrivate(const QString &path);

QString homeDir();

} // namespace paths
} // namespace sheaf
