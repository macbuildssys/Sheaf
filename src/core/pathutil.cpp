#include "pathutil.h"

#include <QDir>
#include <QFile>
#include <QUrl>

namespace sheaf {
namespace pathutil {

QString pathFromUri(const QString &uri)
{
    if (uri.isEmpty() || uri.size() > 4096) {
        return QString();
    }
    const QUrl url(uri, QUrl::TolerantMode);
    if (!url.isValid() || url.scheme() != QLatin1String("file")) {
        return QString();
    }
    if (!url.host().isEmpty() && url.host() != QLatin1String("localhost")) {
        return QString();
    }
    const QString path = url.path(QUrl::FullyDecoded);
    if (path.isEmpty() || !path.startsWith(QLatin1Char('/')) || path.contains(QChar(0))) {
        return QString();
    }
    return QDir::cleanPath(path);
}

QString uriFromPath(const QString &path)
{
    return QString::fromLatin1(QUrl::fromLocalFile(path).toEncoded());
}

QString cleanAbsolute(const QString &path)
{
    if (path.isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(QDir::isAbsolutePath(path) ? path : QDir::current().absoluteFilePath(path));
}

bool isUnder(const QString &path, const QString &dir)
{
    if (dir.isEmpty()) {
        return false;
    }
    QString base = QDir::cleanPath(dir);
    if (base == QLatin1String("/")) {
        return path.startsWith(QLatin1Char('/'));
    }
    return path == base || path.startsWith(base + QLatin1Char('/'));
}

QString expandHome(const QString &path)
{
    if (path == QLatin1String("~")) {
        return QDir::homePath();
    }
    if (path.startsWith(QLatin1String("~/"))) {
        return QDir::homePath() + path.mid(1);
    }
    return path;
}

static QString decodeMountEscapes(const QByteArray &raw)
{
    QByteArray out;
    for (int i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 3 < raw.size() && raw[i + 1] >= '0' && raw[i + 1] <= '3') {
            bool ok = false;
            const int value = raw.mid(i + 1, 3).toInt(&ok, 8);
            if (ok) {
                out.append(static_cast<char>(value));
                i += 3;
                continue;
            }
        }
        out.append(raw[i]);
    }
    return QString::fromUtf8(out);
}

static bool isNetworkFsType(const QByteArray &type)
{
    static const char *const exact[] = {"nfs", "nfs4", "cifs", "smb3", "smbfs", "afs", "ceph", "glusterfs",
                                        "lustre", "9p", "davfs", "ncpfs", "coda", "gpfs", "beegfs"};
    for (const char *name : exact) {
        if (type == name) {
            return true;
        }
    }
    static const char *const prefixes[] = {"fuse.sshfs", "fuse.rclone", "fuse.gvfsd", "fuse.s3fs", "fuse.davfs",
                                           "fuse.curlftpfs", "fuse.smbnetfs", "fuse.gdrive", "fuse.google", "fuse.onedrive"};
    for (const char *prefix : prefixes) {
        if (type.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

QStringList parseNetworkMounts(const QByteArray &mountInfo)
{
    QStringList result;
    const QList<QByteArray> lines = mountInfo.split('\n');
    for (const QByteArray &line : lines) {
        const int sep = line.indexOf(" - ");
        if (sep < 0) {
            continue;
        }
        const QList<QByteArray> head = line.left(sep).split(' ');
        const QList<QByteArray> tail = line.mid(sep + 3).split(' ');
        if (head.size() < 5 || tail.isEmpty()) {
            continue;
        }
        if (isNetworkFsType(tail.first())) {
            result << decodeMountEscapes(head.at(4));
        }
    }
    return result;
}

QStringList networkMountPoints(const QString &mountInfoFile)
{
    QFile file(mountInfoFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return parseNetworkMounts(file.read(4 * 1024 * 1024));
}

bool isOnAnyMount(const QString &path, const QStringList &mountPoints)
{
    for (const QString &mount : mountPoints) {
        if (isUnder(path, mount)) {
            return true;
        }
    }
    return false;
}

} // namespace pathutil
} // namespace sheaf
