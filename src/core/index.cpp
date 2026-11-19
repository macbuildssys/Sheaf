#include "index.h"

#include "pathutil.h"
#include "search.h"
#include "xbel.h"

#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>

namespace sheaf {

DocumentIndex::DocumentIndex(Database *database, QObject *parent)
    : QObject(parent)
    , m_db(database)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, &DocumentIndex::refresh);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &DocumentIndex::onWatchedChange);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &DocumentIndex::onWatchedChange);
    connect(&m_scan, &QFutureWatcher<ScanResult>::finished, this, &DocumentIndex::onScanFinished);

    // Safety net for systems where file watching is limited
    m_poll.setInterval(5 * 60 * 1000);
    connect(&m_poll, &QTimer::timeout, this, &DocumentIndex::checkSignature);
}

DocumentIndex::~DocumentIndex()
{
    if (m_scan.isRunning()) {
        m_scan.waitForFinished();
    }
}

void DocumentIndex::setOptions(const ScanOptions &options)
{
    m_options = options;
    if (m_started) {
        rewatch();
        refresh();
    }
}

void DocumentIndex::start()
{
    m_started = true;
    rewatch();
    m_poll.start();
    refresh();
}

QString DocumentIndex::signature() const
{
    QStringList parts;
    const QFileInfo recent(m_options.recentFile);
    parts << QString::number(recent.exists() ? recent.size() : -1) << recent.lastModified().toString(Qt::ISODateWithMs);
    if (m_options.includeKde && !m_options.kdeDir.isEmpty()) {
        const QFileInfo kde(m_options.kdeDir);
        parts << kde.lastModified().toString(Qt::ISODateWithMs);
    }
    return parts.join(QLatin1Char('|'));
}

void DocumentIndex::rewatch()
{
    const QStringList old = m_watcher.files() + m_watcher.directories();
    if (!old.isEmpty()) {
        m_watcher.removePaths(old);
    }
    if (m_options.recentFile.isEmpty()) {
        return;
    }
    // The file is replaced on every write, so the folder is watched as well
    const QString dir = QFileInfo(m_options.recentFile).absolutePath();
    if (QFileInfo::exists(dir)) {
        m_watcher.addPath(dir);
    }
    if (QFileInfo::exists(m_options.recentFile)) {
        m_watcher.addPath(m_options.recentFile);
    }
    if (m_options.includeKde && QFileInfo::exists(m_options.kdeDir)) {
        m_watcher.addPath(m_options.kdeDir);
    }
}

void DocumentIndex::onWatchedChange()
{
    // Watches on a replaced file are dropped by the kernel, so set them up again
    if (!m_watcher.files().contains(m_options.recentFile) && QFileInfo::exists(m_options.recentFile)) {
        m_watcher.addPath(m_options.recentFile);
    }
    checkSignature();
}

void DocumentIndex::checkSignature()
{
    if (signature() != m_signature) {
        m_debounce.start();
    }
}

ScanInput DocumentIndex::makeInput() const
{
    ScanInput input;
    input.options = m_options;
    input.favorites = m_db->favorites();
    input.tags = m_db->tags();
    input.history = m_db->history();
    return input;
}

void DocumentIndex::refresh()
{
    if (m_busy) {
        m_again = true;
        return;
    }
    m_busy = true;
    m_again = false;
    m_signature = signature();
    emit busyChanged(true);
    const ScanInput input = makeInput();
    m_scan.setFuture(QtConcurrent::run([input]() { return scanDocuments(input); }));
}

void DocumentIndex::onScanFinished()
{
    const ScanResult result = m_scan.result();
    m_docs = result.documents;
    m_busy = false;
    emit busyChanged(false);
    emit documentsChanged();
    for (const QString &message : result.warnings) {
        emit warning(message);
    }
    if (m_again) {
        refresh();
    }
}

const DocumentInfo *DocumentIndex::find(const QString &key) const
{
    for (const DocumentInfo &doc : m_docs) {
        if (doc.key == key) {
            return &doc;
        }
    }
    return nullptr;
}

void DocumentIndex::applyUserData()
{
    const QVector<FavoriteRow> favorites = m_db->favorites();
    const QHash<QString, QStringList> tags = m_db->tags();
    const QHash<QString, HistoryRow> history = m_db->history();
    QHash<QString, FavoriteRow> favoriteMap;
    for (const FavoriteRow &row : favorites) {
        favoriteMap.insert(row.key, row);
    }

    bool needScan = false;
    QSet<QString> present;
    QVector<DocumentInfo> kept;
    kept.reserve(m_docs.size());
    for (DocumentInfo doc : std::as_const(m_docs)) {
        present.insert(doc.key);
        const auto fav = favoriteMap.constFind(doc.key);
        doc.favorite = fav != favoriteMap.constEnd();
        doc.favoriteAdded = doc.favorite ? fav->added : QDateTime();
        doc.tags = tags.value(doc.key);
        const auto hist = history.constFind(doc.key);
        doc.openCount = hist != history.constEnd() ? hist->count : 0;
        if (hist != history.constEnd() && (!doc.lastUsed.isValid() || hist->lastOpened > doc.lastUsed)) {
            doc.lastUsed = hist->lastOpened;
        }
        doc.haystack = SearchQuery::buildHaystack(doc);
        if (!doc.favorite && !doc.inRecents && hist == history.constEnd()) {
            continue;
        }
        kept.append(doc);
    }
    for (const FavoriteRow &row : favorites) {
        if (!present.contains(row.key)) {
            needScan = true;
        }
    }
    for (auto it = history.constBegin(); it != history.constEnd(); ++it) {
        if (!present.contains(it.key())) {
            needScan = true;
        }
    }
    m_docs = kept;
    emit documentsChanged();
    if (needScan) {
        refresh();
    }
}

bool DocumentIndex::setFavorite(const DocumentInfo &doc, bool on)
{
    const bool ok = m_db->setFavorite(doc.key, doc.uri, on);
    applyUserData();
    return ok;
}

bool DocumentIndex::setTags(const DocumentInfo &doc, const QStringList &tags)
{
    const bool ok = m_db->setTags(doc.key, tags);
    applyUserData();
    return ok;
}

bool DocumentIndex::recordOpen(const DocumentInfo &doc)
{
    const bool ok = m_db->recordOpen(doc.key);
    applyUserData();
    return ok;
}

bool DocumentIndex::removeFromRecents(const QList<DocumentInfo> &docs, QString *error)
{
    QSet<QString> paths;
    QStringList keys;
    for (const DocumentInfo &doc : docs) {
        paths.insert(doc.path);
        paths.insert(pathutil::pathFromUri(doc.uri));
        for (const QString &alias : doc.aliases) {
            paths.insert(alias);
        }
        keys << doc.key;
    }
    paths.remove(QString());
    int removed = 0;
    bool ok = xbel::removePaths(m_options.recentFile, paths, &removed, error);
    ok = m_db->forget(keys, false) && ok;

    // Documents that only came from recents disappear, favorites stay
    for (DocumentInfo &doc : m_docs) {
        if (keys.contains(doc.key)) {
            doc.inRecents = false;
        }
    }
    applyUserData();
    return ok;
}

bool DocumentIndex::clearRecents(QString *error)
{
    bool ok = xbel::clear(m_options.recentFile, error);
    ok = m_db->clearHistory() && ok;
    for (DocumentInfo &doc : m_docs) {
        doc.inRecents = false;
    }
    applyUserData();
    return ok;
}

bool DocumentIndex::removeEverything(const QList<DocumentInfo> &docs, QString *error)
{
    if (docs.isEmpty()) {
        return true;
    }
    QStringList keys;
    for (const DocumentInfo &doc : docs) {
        keys << doc.key;
    }
    const bool ok = removeFromRecents(docs, error);
    m_db->forget(keys, true);
    QVector<DocumentInfo> kept;
    for (const DocumentInfo &doc : std::as_const(m_docs)) {
        if (!keys.contains(doc.key)) {
            kept.append(doc);
        }
    }
    m_docs = kept;
    emit documentsChanged();
    return ok;
}

int DocumentIndex::removeMissing(QString *error)
{
    QList<DocumentInfo> missing;
    for (const DocumentInfo &doc : std::as_const(m_docs)) {
        if (!doc.exists) {
            missing << doc;
        }
    }
    removeEverything(missing, error);
    return missing.size();
}

bool DocumentIndex::relocate(const DocumentInfo &doc, const QString &newPath)
{
    const QString newKey = documentKey(newPath, pathutil::networkMountPoints());
    if (!m_db->relocate(doc.key, newKey, pathutil::uriFromPath(newKey))) {
        return false;
    }
    QString ignored;
    removeFromRecents({doc}, &ignored);
    refresh();
    return true;
}

} // namespace sheaf
