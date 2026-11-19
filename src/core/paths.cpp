#include "paths.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace sheaf {
namespace paths {

QString homeDir()
{
    return QDir::homePath();
}

QString dataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/sheaf");
}

QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/sheaf");
}

QString databaseFile()
{
    return dataDir() + QStringLiteral("/library.db");
}

QString groupsFile()
{
    return configDir() + QStringLiteral("/groups.json");
}

QString settingsFile()
{
    return configDir() + QStringLiteral("/sheaf.conf");
}

QString recentFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/recently-used.xbel");
}

QString kdeRecentDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/RecentDocuments");
}

QString thumbnailRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/thumbnails");
}

QString trashFilesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash/files");
}

bool ensurePrivateDir(const QString &path)
{
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return true;
}

void makePrivate(const QString &path)
{
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

} // namespace paths
} // namespace sheaf
