#include "thingyimport.h"

#include "paths.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace sheaf {
namespace thingy {

QString groupsFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/thingy/groups.json");
}

QStringList parseGsettingsFavorites(const QString &text)
{
    // Each entry looks like 'file:///path/doc.pdf::application/pdf'
    QStringList uris;
    static const QRegularExpression entry(QStringLiteral("'((?:[^'\\\\]|\\\\.)*)'"));
    QRegularExpressionMatchIterator it = entry.globalMatch(text.left(4 * 1024 * 1024));
    while (it.hasNext()) {
        QString value = it.next().captured(1);
        value.replace(QStringLiteral("\\'"), QStringLiteral("'")).replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
        const int delimiter = value.indexOf(QStringLiteral("::"));
        const QString uri = delimiter > 0 ? value.left(delimiter) : value;
        if (uri.startsWith(QLatin1String("file://")) && !uris.contains(uri)) {
            uris << uri;
        }
    }
    return uris;
}

bool readFavorites(QStringList *uris, QString *error)
{
    const QString program = QStandardPaths::findExecutable(QStringLiteral("gsettings"));
    if (program.isEmpty()) {
        if (error) {
            *error = QStringLiteral("gsettings is not installed");
        }
        return false;
    }
    QProcess process;
    process.start(program, {QStringLiteral("get"), QStringLiteral("org.x.apps.favorites"), QStringLiteral("list")});
    if (!process.waitForFinished(5000) || process.exitCode() != 0) {
        process.kill();
        if (error) {
            *error = QStringLiteral("Thingy favorites are not available on this system");
        }
        return false;
    }
    *uris = parseGsettingsFavorites(QString::fromUtf8(process.readAllStandardOutput()));
    return true;
}

} // namespace thingy
} // namespace sheaf
