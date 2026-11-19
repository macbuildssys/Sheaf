#include "scanner.h"

#include "pathutil.h"
#include "progress.h"
#include "search.h"
#include "xbel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

#include <algorithm>

namespace sheaf {

namespace {

struct Candidate {
    QString path;
    QString uri;
    QString mime;
    QString originalPath;
    QDateTime used;
    QDateTime favoriteAdded;
    QStringList apps;
    QStringList aliases;
    bool inRecents = false;
    bool favorite = false;
    bool remote = false;
};

QDateTime newest(const QDateTime &a, const QDateTime &b)
{
    if (!a.isValid()) {
        return b;
    }
    if (!b.isValid()) {
        return a;
    }
    return a > b ? a : b;
}

// Reads the URL line of a KDE recent document entry
QString kdeEntryTarget(const QString &file)
{
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly) || in.size() > 64 * 1024) {
        return QString();
    }
    const QList<QByteArray> lines = in.readAll().split('\n');
    for (const QByteArray &line : lines) {
        if (line.startsWith("URL=")) {
            const QString value = QString::fromUtf8(line.mid(4)).trimmed();
            if (value.startsWith(QLatin1Char('/'))) {
                return QDir::cleanPath(value);
            }
            return pathutil::pathFromUri(value);
        }
    }
    return QString();
}

QStringList mergeApps(QStringList base, const QStringList &extra)
{
    for (const QString &app : extra) {
        if (!base.contains(app) && base.size() < 16) {
            base << app;
        }
    }
    return base;
}

bool excluded(const QString &path, const QStringList &exclusions)
{
    for (const QString &dir : exclusions) {
        if (pathutil::isUnder(path, dir)) {
            return true;
        }
    }
    return false;
}

} // namespace

QStringList defaultExclusions()
{
    return {QStringLiteral("/tmp"), QStringLiteral("/var/tmp"), QDir::homePath() + QStringLiteral("/.cache"),
            QDir::homePath() + QStringLiteral("/.local/share/Trash")};
}

QString documentKey(const QString &path, const QStringList &networkMounts)
{
    const QString clean = pathutil::cleanAbsolute(path);
    if (clean.isEmpty() || pathutil::isOnAnyMount(clean, networkMounts)) {
        return clean;
    }
    const QString canonical = QFileInfo(clean).canonicalFilePath();
    return canonical.isEmpty() ? clean : canonical;
}

ScanResult scanDocuments(const ScanInput &input)
{
    ScanResult result;
    const ScanOptions &opt = input.options;
    const QStringList mounts = !opt.networkMounts.isEmpty() ? opt.networkMounts
                               : opt.autoDetectNetwork       ? pathutil::networkMountPoints()
                                                             : QStringList();
    QStringList exclusions;
    for (const QString &dir : opt.exclusions) {
        const QString clean = pathutil::cleanAbsolute(pathutil::expandHome(dir));
        if (!clean.isEmpty()) {
            exclusions << clean;
        }
    }

    QHash<QString, Candidate> candidates;
    auto candidateFor = [&](const QString &path) -> Candidate & {
        const QString key = documentKey(path, mounts);
        auto it = candidates.find(key);
        if (it == candidates.end()) {
            it = candidates.insert(key, Candidate());
            it->path = key;
            it->originalPath = pathutil::cleanAbsolute(path);
            it->remote = pathutil::isOnAnyMount(key, mounts);
        }
        const QString original = pathutil::cleanAbsolute(path);
        if (original != key && !it->aliases.contains(original) && it->aliases.size() < 16) {
            it->aliases << original;
        }
        return *it;
    };

    const XbelResult recent = xbel::readFile(opt.recentFile);
    if (!recent.error.isEmpty()) {
        result.warnings << QStringLiteral("Recent documents: %1").arg(recent.error);
    }
    if (recent.truncated) {
        result.warnings << QStringLiteral("The recent documents list is very long, only part of it is shown");
    }
    for (const RecentEntry &entry : recent.entries) {
        // Entries marked private belong to the app that created them
        if (entry.isPrivate) {
            continue;
        }
        const QString path = pathutil::pathFromUri(entry.uri);
        if (path.isEmpty()) {
            continue;
        }
        Candidate &c = candidateFor(path);
        c.inRecents = true;
        if (c.uri.isEmpty()) {
            c.uri = entry.uri;
        }
        if (c.mime.isEmpty()) {
            c.mime = entry.mime;
        }
        c.used = newest(c.used, newest(entry.visited, newest(entry.modified, entry.added)));
        c.apps = mergeApps(c.apps, entry.apps);
    }

    if (opt.includeKde && !opt.kdeDir.isEmpty()) {
        const QFileInfoList files = QDir(opt.kdeDir).entryInfoList({QStringLiteral("*.desktop")}, QDir::Files | QDir::Readable,
                                                                    QDir::Time);
        int seen = 0;
        for (const QFileInfo &fi : files) {
            if (++seen > 5000) {
                break;
            }
            const QString path = kdeEntryTarget(fi.absoluteFilePath());
            if (path.isEmpty()) {
                continue;
            }
            Candidate &c = candidateFor(path);
            c.inRecents = true;
            c.used = newest(c.used, fi.lastModified());
        }
    }

    for (const FavoriteRow &row : input.favorites) {
        if (!QDir::isAbsolutePath(row.key)) {
            continue;
        }
        Candidate &c = candidateFor(row.key);
        c.favorite = true;
        c.favoriteAdded = row.added;
        if (c.uri.isEmpty()) {
            c.uri = row.uri;
        }
    }
    for (auto it = input.history.constBegin(); it != input.history.constEnd(); ++it) {
        if (!QDir::isAbsolutePath(it.key())) {
            continue;
        }
        Candidate &c = candidateFor(it.key());
        c.inRecents = true;
        c.used = newest(c.used, it.value().lastOpened);
    }

    QMimeDatabase mimeDb;
    QVector<DocumentInfo> docs;
    docs.reserve(candidates.size());
    for (auto it = candidates.constBegin(); it != candidates.constEnd(); ++it) {
        const Candidate &c = it.value();
        if (!c.favorite && (excluded(c.originalPath, exclusions) || excluded(c.path, exclusions))) {
            continue;
        }

        DocumentInfo doc;
        doc.key = c.path;
        doc.path = c.path;
        doc.uri = c.uri.isEmpty() ? pathutil::uriFromPath(c.path) : c.uri;
        doc.favorite = c.favorite;
        doc.favoriteAdded = c.favoriteAdded;
        doc.inRecents = c.inRecents;
        doc.remote = c.remote;
        doc.apps = c.apps;
        doc.aliases = c.aliases;
        doc.lastUsed = c.used;

        const QFileInfo fi(c.path);
        doc.name = fi.fileName();
        doc.extension = fi.suffix().toLower();

        if (c.remote) {
            // Never touch a network file system while scanning, it may be unreachable
            doc.exists = true;
        } else {
            doc.exists = fi.exists();
            if (doc.exists && !fi.isFile()) {
                continue;
            }
        }
        if (!doc.exists && !opt.showMissing && !doc.favorite) {
            continue;
        }
        if (doc.exists && !c.remote) {
            doc.size = fi.size();
            doc.modified = fi.lastModified();
            if (!doc.lastUsed.isValid()) {
                doc.lastUsed = doc.modified;
            }
        }

        QMimeType type = mimeDb.mimeTypeForFile(c.path, QMimeDatabase::MatchExtension);
        const bool generic = !type.isValid() || type.isDefault();
        if (generic && doc.exists && !c.remote) {
            type = mimeDb.mimeTypeForFile(fi, QMimeDatabase::MatchDefault);
        }
        if ((!type.isValid() || type.isDefault()) && !c.mime.isEmpty()) {
            const QMimeType hinted = mimeDb.mimeTypeForName(c.mime);
            if (hinted.isValid()) {
                type = hinted;
            }
        }
        doc.mime = type.isValid() ? type.name() : QStringLiteral("application/octet-stream");
        doc.mimeComment = type.isValid() ? type.comment() : QString();

        doc.tags = input.tags.value(c.path);
        const auto history = input.history.constFind(c.path);
        if (history != input.history.constEnd()) {
            doc.openCount = history->count;
        }
        docs.append(doc);
    }

    std::sort(docs.begin(), docs.end(), [](const DocumentInfo &a, const DocumentInfo &b) {
        if (a.lastUsed != b.lastUsed) {
            return a.lastUsed > b.lastUsed;
        }
        return a.key < b.key;
    });

    // Favorites are never cut, the cap only trims plain recents
    if (opt.maxItems > 0) {
        int kept = 0;
        QVector<DocumentInfo> limited;
        limited.reserve(std::min<int>(docs.size(), opt.maxItems + 64));
        for (const DocumentInfo &doc : std::as_const(docs)) {
            if (doc.favorite || kept < opt.maxItems) {
                limited.append(doc);
                if (!doc.favorite) {
                    ++kept;
                }
            }
        }
        docs = limited;
    }

    if (opt.readProgress && progressSupported()) {
        int budget = 3000;
        for (DocumentInfo &doc : docs) {
            if (budget <= 0) {
                break;
            }
            if (doc.exists && !doc.remote && progressCandidate(doc.mime)) {
                --budget;
                const ProgressInfo info = readReadingProgress(doc.path);
                doc.progress = info.fraction;
                doc.title = info.title;
                doc.author = info.author;
            }
        }
    }

    for (DocumentInfo &doc : docs) {
        doc.haystack = SearchQuery::buildHaystack(doc);
    }
    result.documents = docs;
    return result;
}

} // namespace sheaf
