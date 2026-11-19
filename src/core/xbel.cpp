#include "xbel.h"

#include "pathutil.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace sheaf {
namespace xbel {

static QDateTime parseTime(const QString &text)
{
    if (text.isEmpty()) {
        return QDateTime();
    }
    return QDateTime::fromString(text, Qt::ISODate);
}

XbelResult readData(const QByteArray &data, int maxEntries)
{
    XbelResult result;
    QXmlStreamReader xml(data);
    xml.setNamespaceProcessing(true);

    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QLatin1String("bookmark")) {
            continue;
        }
        if (result.entries.size() >= maxEntries) {
            result.truncated = true;
            break;
        }
        RecentEntry entry;
        entry.uri = xml.attributes().value(QLatin1String("href")).toString();
        entry.added = parseTime(xml.attributes().value(QLatin1String("added")).toString());
        entry.modified = parseTime(xml.attributes().value(QLatin1String("modified")).toString());
        entry.visited = parseTime(xml.attributes().value(QLatin1String("visited")).toString());

        int depth = 1;
        while (!xml.atEnd() && depth > 0) {
            xml.readNext();
            if (xml.isStartElement()) {
                ++depth;
                const QString name = xml.name().toString();
                if (name == QLatin1String("mime-type")) {
                    entry.mime = xml.attributes().value(QLatin1String("type")).toString();
                } else if (name == QLatin1String("private")) {
                    entry.isPrivate = true;
                } else if (name == QLatin1String("application")) {
                    const QString app = xml.attributes().value(QLatin1String("name")).toString();
                    if (!app.isEmpty() && entry.apps.size() < 16) {
                        entry.apps << app;
                    }
                }
            } else if (xml.isEndElement()) {
                --depth;
            }
        }
        if (!entry.uri.isEmpty()) {
            result.entries.append(entry);
        }
    }

    if (xml.hasError() && !result.truncated) {
        result.error = xml.errorString();
    }
    return result;
}

XbelResult readFile(const QString &path)
{
    XbelResult result;
    QFile file(path);
    if (!file.exists()) {
        return result;
    }
    if (file.size() > kMaxFileBytes) {
        result.error = QStringLiteral("recently-used.xbel is larger than the %1 MB limit").arg(kMaxFileBytes / (1024 * 1024));
        return result;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
    return readData(file.readAll());
}

QByteArray emptyDocument()
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<xbel version=\"1.0\"\n"
        "      xmlns:bookmark=\"http://www.freedesktop.org/standards/desktop-bookmarks\"\n"
        "      xmlns:mime=\"http://www.freedesktop.org/standards/shared-mime-info\"\n"
        ">\n"
        "</xbel>\n");
}

static bool writeAtomically(const QString &file, const QByteArray &data, QString *error)
{
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = out.errorString();
        }
        return false;
    }
    out.write(data);
    if (!out.commit()) {
        if (error) {
            *error = out.errorString();
        }
        return false;
    }
    return true;
}

bool removePaths(const QString &file, const QSet<QString> &paths, int *removed, QString *error)
{
    if (removed) {
        *removed = 0;
    }
    if (paths.isEmpty()) {
        return true;
    }
    QFile in(file);
    if (!in.exists()) {
        return true;
    }
    if (in.size() > kMaxFileBytes || !in.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = in.errorString();
        }
        return false;
    }
    const QByteArray original = in.readAll();
    in.close();

    QByteArray output;
    QBuffer buffer(&output);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.setAutoFormatting(false);

    QXmlStreamReader reader(original);
    reader.setNamespaceProcessing(true);
    int count = 0;

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.hasError()) {
            break;
        }
        if (reader.isStartElement() && reader.name() == QLatin1String("bookmark")) {
            const QString href = reader.attributes().value(QLatin1String("href")).toString();
            if (paths.contains(pathutil::pathFromUri(href))) {
                int depth = 1;
                while (!reader.atEnd() && depth > 0) {
                    reader.readNext();
                    if (reader.isStartElement()) {
                        ++depth;
                    } else if (reader.isEndElement()) {
                        --depth;
                    }
                }
                ++count;
                continue;
            }
        }
        if (reader.tokenType() != QXmlStreamReader::Invalid) {
            writer.writeCurrentToken(reader);
        }
    }

    if (reader.hasError()) {
        if (error) {
            *error = QStringLiteral("The recent documents file is damaged: %1").arg(reader.errorString());
        }
        return false;
    }
    buffer.close();

    // Never replace the user's file with something that does not parse
    const XbelResult check = readData(output);
    if (!check.error.isEmpty()) {
        if (error) {
            *error = check.error;
        }
        return false;
    }
    if (count == 0) {
        return true;
    }
    if (!writeAtomically(file, output, error)) {
        return false;
    }
    if (removed) {
        *removed = count;
    }
    return true;
}

bool clear(const QString &file, QString *error)
{
    return writeAtomically(file, emptyDocument(), error);
}

} // namespace xbel
} // namespace sheaf
