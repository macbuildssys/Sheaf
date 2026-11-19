#include "database.h"

#include "paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <atomic>

namespace sheaf {

namespace {

std::atomic<int> g_connectionCounter{0};

constexpr int kSchemaVersion = 1;

QSqlQuery makeQuery(const QString &connection)
{
    return QSqlQuery(QSqlDatabase::database(connection, false));
}

} // namespace

Database::Database()
    : m_connection(QStringLiteral("sheaf-db-%1").arg(g_connectionCounter.fetch_add(1)))
{
}

Database::~Database()
{
    close();
}

void Database::close()
{
    if (!m_open && !QSqlDatabase::contains(m_connection)) {
        return;
    }
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(m_connection);
    m_open = false;
}

bool Database::exec(const QString &sql, const QVariantList &values) const
{
    QSqlQuery query = makeQuery(m_connection);
    query.prepare(sql);
    for (const QVariant &value : values) {
        query.addBindValue(value);
    }
    return query.exec();
}

bool Database::open(const QString &file, QString *error)
{
    close();
    m_file = file;
    m_recoveryNote.clear();
    m_newerSchema = false;

    if (!paths::ensurePrivateDir(QFileInfo(file).absolutePath())) {
        if (error) {
            *error = QStringLiteral("Could not create %1").arg(QFileInfo(file).absolutePath());
        }
        return false;
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        bool healthy = false;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
            db.setDatabaseName(file);
            healthy = db.open();
            if (healthy) {
                QSqlQuery check = makeQuery(m_connection);
                healthy = check.exec(QStringLiteral("PRAGMA quick_check")) && check.next() &&
                          check.value(0).toString() == QLatin1String("ok");
            }
            if (healthy) {
                QString migrateError;
                healthy = migrate(&migrateError);
                if (!healthy && error) {
                    *error = migrateError;
                }
            }
            if (!healthy) {
                db.close();
            }
        }
        if (healthy) {
            m_open = true;
            paths::makePrivate(file);
            return true;
        }

        QSqlDatabase::removeDatabase(m_connection);
        if (attempt == 0 && !m_newerSchema && QFile::exists(file)) {
            const QString aside = file + QStringLiteral(".corrupt-") +
                                  QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmss"));
            if (QFile::rename(file, aside)) {
                m_recoveryNote = QStringLiteral("The library database was damaged and has been moved to %1. A new one was created.").arg(aside);
                continue;
            }
        }
        break;
    }
    if (error && error->isEmpty()) {
        *error = QStringLiteral("Could not open the library database at %1").arg(file);
    }
    return false;
}

bool Database::migrate(QString *error)
{
    QSqlQuery query = makeQuery(m_connection);
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    query.exec(QStringLiteral("PRAGMA foreign_keys=ON"));

    int version = 0;
    if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next()) {
        version = query.value(0).toInt();
    }
    if (version > kSchemaVersion) {
        m_newerSchema = true;
        if (error) {
            *error = QStringLiteral("The library database was created by a newer version of Sheaf");
        }
        return false;
    }
    if (version < 1) {
        const QStringList statements = {
            QStringLiteral("CREATE TABLE IF NOT EXISTS favorites(key TEXT PRIMARY KEY NOT NULL, uri TEXT NOT NULL, added INTEGER NOT NULL)"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS tags(key TEXT NOT NULL, norm TEXT NOT NULL, tag TEXT NOT NULL, PRIMARY KEY(key, norm))"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS tags_by_norm ON tags(norm)"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS history(key TEXT PRIMARY KEY NOT NULL, last_opened INTEGER NOT NULL, open_count INTEGER NOT NULL)"),
        };
        exec(QStringLiteral("BEGIN"));
        for (const QString &sql : statements) {
            if (!exec(sql)) {
                exec(QStringLiteral("ROLLBACK"));
                if (error) {
                    *error = QStringLiteral("Could not create the library schema");
                }
                return false;
            }
        }
        exec(QStringLiteral("PRAGMA user_version=%1").arg(kSchemaVersion));
        exec(QStringLiteral("COMMIT"));
    }
    return true;
}

QVector<FavoriteRow> Database::favorites() const
{
    QVector<FavoriteRow> rows;
    QSqlQuery query = makeQuery(m_connection);
    if (query.exec(QStringLiteral("SELECT key, uri, added FROM favorites ORDER BY added DESC"))) {
        while (query.next()) {
            FavoriteRow row;
            row.key = query.value(0).toString();
            row.uri = query.value(1).toString();
            row.added = QDateTime::fromSecsSinceEpoch(query.value(2).toLongLong(), Qt::UTC);
            rows.append(row);
        }
    }
    return rows;
}

bool Database::setFavorite(const QString &key, const QString &uri, bool on)
{
    if (key.isEmpty()) {
        return false;
    }
    if (on) {
        return exec(QStringLiteral("INSERT OR IGNORE INTO favorites(key, uri, added) VALUES(?, ?, ?)"),
                    {key, uri, QDateTime::currentSecsSinceEpoch()});
    }
    return exec(QStringLiteral("DELETE FROM favorites WHERE key = ?"), {key});
}

QString Database::normalizeTag(const QString &tag)
{
    QString text = tag.simplified();
    if (text.isEmpty() || text.size() > 64) {
        return QString();
    }
    for (const QChar ch : text) {
        if (ch.category() == QChar::Other_Control || ch == QLatin1Char(',')) {
            return QString();
        }
    }
    return text;
}

QHash<QString, QStringList> Database::tags() const
{
    QHash<QString, QStringList> map;
    QSqlQuery query = makeQuery(m_connection);
    if (query.exec(QStringLiteral("SELECT key, tag FROM tags ORDER BY norm"))) {
        while (query.next()) {
            map[query.value(0).toString()].append(query.value(1).toString());
        }
    }
    return map;
}

QStringList Database::allTagNames() const
{
    QStringList names;
    QSqlQuery query = makeQuery(m_connection);
    if (query.exec(QStringLiteral("SELECT MIN(tag) FROM tags GROUP BY norm ORDER BY norm"))) {
        while (query.next()) {
            names << query.value(0).toString();
        }
    }
    return names;
}

bool Database::addTag(const QString &key, const QString &tag)
{
    const QString clean = normalizeTag(tag);
    if (key.isEmpty() || clean.isEmpty()) {
        return false;
    }
    return exec(QStringLiteral("INSERT OR IGNORE INTO tags(key, norm, tag) VALUES(?, ?, ?)"), {key, clean.toCaseFolded(), clean});
}

bool Database::removeTag(const QString &key, const QString &tag)
{
    return exec(QStringLiteral("DELETE FROM tags WHERE key = ? AND norm = ?"), {key, tag.simplified().toCaseFolded()});
}

bool Database::setTags(const QString &key, const QStringList &tags)
{
    if (key.isEmpty()) {
        return false;
    }
    exec(QStringLiteral("BEGIN"));
    bool ok = exec(QStringLiteral("DELETE FROM tags WHERE key = ?"), {key});
    for (const QString &tag : tags) {
        if (!normalizeTag(tag).isEmpty()) {
            ok = addTag(key, tag) && ok;
        }
    }
    exec(ok ? QStringLiteral("COMMIT") : QStringLiteral("ROLLBACK"));
    return ok;
}

QHash<QString, HistoryRow> Database::history() const
{
    QHash<QString, HistoryRow> map;
    QSqlQuery query = makeQuery(m_connection);
    if (query.exec(QStringLiteral("SELECT key, last_opened, open_count FROM history"))) {
        while (query.next()) {
            HistoryRow row;
            row.key = query.value(0).toString();
            row.lastOpened = QDateTime::fromSecsSinceEpoch(query.value(1).toLongLong(), Qt::UTC);
            row.count = query.value(2).toInt();
            map.insert(row.key, row);
        }
    }
    return map;
}

bool Database::recordOpen(const QString &key, const QDateTime &when)
{
    if (key.isEmpty()) {
        return false;
    }
    return exec(QStringLiteral("INSERT INTO history(key, last_opened, open_count) VALUES(?, ?, 1) "
                               "ON CONFLICT(key) DO UPDATE SET last_opened = excluded.last_opened, open_count = open_count + 1"),
                {key, when.toSecsSinceEpoch()});
}

bool Database::clearHistory()
{
    return exec(QStringLiteral("DELETE FROM history"));
}

bool Database::forget(const QStringList &keys, bool everything)
{
    exec(QStringLiteral("BEGIN"));
    bool ok = true;
    for (const QString &key : keys) {
        ok = exec(QStringLiteral("DELETE FROM history WHERE key = ?"), {key}) && ok;
        if (everything) {
            ok = exec(QStringLiteral("DELETE FROM favorites WHERE key = ?"), {key}) && ok;
            ok = exec(QStringLiteral("DELETE FROM tags WHERE key = ?"), {key}) && ok;
        }
    }
    exec(ok ? QStringLiteral("COMMIT") : QStringLiteral("ROLLBACK"));
    return ok;
}

bool Database::relocate(const QString &oldKey, const QString &newKey, const QString &newUri)
{
    if (oldKey.isEmpty() || newKey.isEmpty() || oldKey == newKey) {
        return false;
    }
    exec(QStringLiteral("BEGIN"));
    bool ok = exec(QStringLiteral("UPDATE OR REPLACE favorites SET key = ?, uri = ? WHERE key = ?"), {newKey, newUri, oldKey});
    ok = exec(QStringLiteral("UPDATE OR REPLACE tags SET key = ? WHERE key = ?"), {newKey, oldKey}) && ok;
    ok = exec(QStringLiteral("UPDATE OR REPLACE history SET key = ? WHERE key = ?"), {newKey, oldKey}) && ok;
    exec(ok ? QStringLiteral("COMMIT") : QStringLiteral("ROLLBACK"));
    return ok;
}

QJsonObject Database::exportJson() const
{
    QJsonArray favorites;
    for (const FavoriteRow &row : this->favorites()) {
        QJsonObject object;
        object.insert(QStringLiteral("path"), row.key);
        object.insert(QStringLiteral("uri"), row.uri);
        object.insert(QStringLiteral("added"), row.added.toSecsSinceEpoch());
        favorites.append(object);
    }
    QJsonArray tags;
    const QHash<QString, QStringList> all = this->tags();
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
        QJsonObject object;
        object.insert(QStringLiteral("path"), it.key());
        object.insert(QStringLiteral("tags"), QJsonArray::fromStringList(it.value()));
        tags.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("sheaf-library"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("favorites"), favorites);
    root.insert(QStringLiteral("tags"), tags);
    return root;
}

bool Database::importJson(const QJsonObject &root, int *imported, QString *error)
{
    if (imported) {
        *imported = 0;
    }
    if (root.value(QLatin1String("format")).toString() != QLatin1String("sheaf-library")) {
        if (error) {
            *error = QStringLiteral("This is not a Sheaf library export");
        }
        return false;
    }
    int count = 0;
    exec(QStringLiteral("BEGIN"));
    const QJsonArray favorites = root.value(QLatin1String("favorites")).toArray();
    for (const QJsonValue &value : favorites) {
        const QJsonObject object = value.toObject();
        const QString path = object.value(QLatin1String("path")).toString();
        if (!QDir::isAbsolutePath(path)) {
            continue;
        }
        if (exec(QStringLiteral("INSERT OR IGNORE INTO favorites(key, uri, added) VALUES(?, ?, ?)"),
                 {path, object.value(QLatin1String("uri")).toString(),
                  static_cast<qint64>(object.value(QLatin1String("added")).toDouble(QDateTime::currentSecsSinceEpoch()))})) {
            ++count;
        }
    }
    const QJsonArray tags = root.value(QLatin1String("tags")).toArray();
    for (const QJsonValue &value : tags) {
        const QJsonObject object = value.toObject();
        const QString path = object.value(QLatin1String("path")).toString();
        if (!QDir::isAbsolutePath(path)) {
            continue;
        }
        const QJsonArray list = object.value(QLatin1String("tags")).toArray();
        for (const QJsonValue &tag : list) {
            if (addTag(path, tag.toString())) {
                ++count;
            }
        }
    }
    exec(QStringLiteral("COMMIT"));
    if (imported) {
        *imported = count;
    }
    return true;
}

} // namespace sheaf
