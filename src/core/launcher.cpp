#include "launcher.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace sheaf {
namespace launcher {

bool isBlockedType(const QString &mime, const QString &path)
{
    static const char *const always[] = {
        "application/x-desktop",           "application/x-executable",      "application/x-pie-executable",
        "application/x-sharedlib",         "application/x-msdos-program",   "application/x-ms-dos-executable",
        "application/vnd.microsoft.portable-executable", "application/x-msi", "application/vnd.appimage",
        "application/x-iso9660-appimage",  "application/x-java-archive",    "application/x-ms-shortcut",
    };
    for (const char *name : always) {
        if (mime == QLatin1String(name)) {
            return true;
        }
    }
    // The type in the recent list comes from other programs, so the file name is checked too
    const QFileInfo info(path);
    static const char *const blockedSuffixes[] = {"desktop", "appimage", "exe", "msi", "jar", "lnk", "bat", "com", "scr", "run"};
    const QString suffix = info.suffix().toLower();
    for (const char *name : blockedSuffixes) {
        if (suffix == QLatin1String(name)) {
            return true;
        }
    }
    // Scripts open in an editor, unless they are marked as runnable
    if (info.isExecutable()) {
        const QMimeType type = QMimeDatabase().mimeTypeForName(mime);
        if (type.isValid() && type.inherits(QStringLiteral("application/x-executable"))) {
            return true;
        }
    }
    return false;
}

QString openerProgram()
{
    const QString custom = qEnvironmentVariable("SHEAF_OPENER");
    if (!custom.isEmpty()) {
        return custom;
    }
    return QStringLiteral("xdg-open");
}

static bool startDetachedProgram(const QString &program, const QStringList &args)
{
    const QString resolved = QDir::isAbsolutePath(program) ? program : QStandardPaths::findExecutable(program);
    if (resolved.isEmpty()) {
        return false;
    }
    return QProcess::startDetached(resolved, args);
}

OpenResult open(const QString &path, const QString &mime)
{
    const QFileInfo info(path);
    if (!QDir::isAbsolutePath(path)) {
        return OpenResult::Failed;
    }
    if (!info.exists()) {
        return OpenResult::Missing;
    }
    if (isBlockedType(mime, path)) {
        return OpenResult::Blocked;
    }
    return startDetachedProgram(openerProgram(), {path}) ? OpenResult::Ok : OpenResult::Failed;
}

bool showInFolder(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return false;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
                                                              QStringLiteral("/org/freedesktop/FileManager1"),
                                                              QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
        message << QStringList{QString::fromLatin1(QUrl::fromLocalFile(path).toEncoded())} << QStringLiteral("sheaf");
        const QDBusMessage reply = bus.call(message, QDBus::Block, 1500);
        if (reply.type() == QDBusMessage::ReplyMessage) {
            return true;
        }
    }
    return startDetachedProgram(openerProgram(), {info.absolutePath()});
}

bool moveToTrash(const QString &path, QString *error)
{
    if (!QFile::moveToTrash(path)) {
        if (error) {
            *error = QStringLiteral("Could not move the file to the trash");
        }
        return false;
    }
    return true;
}

} // namespace launcher
} // namespace sheaf
