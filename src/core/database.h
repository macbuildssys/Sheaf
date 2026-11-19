#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace sheaf {

struct FavoriteRow {
    QString key;
    QString uri;
    QDateTime added;
};

struct HistoryRow {
    QString key;
    QDateTime lastOpened;
    int count = 0;
};

// Favorites, tags and the history of documents opened through Sheaf.
// Not thread safe: use it from the thread that opened it
class Database {
public:
    Database();
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(const QString &file, QString *error = nullptr);
    void close();
    bool isOpen() const { return m_open; }

    // Set when a damaged database was moved aside and replaced
    QString recoveryNote() const { return m_recoveryNote; }

    QVector<FavoriteRow> favorites() const;
    bool setFavorite(const QString &key, const QString &uri, bool on);

    QHash<QString, QStringList> tags() const;
    QStringList allTagNames() const;
    bool setTags(const QString &key, const QStringList &tags);
    bool addTag(const QString &key, const QString &tag);
    bool removeTag(const QString &key, const QString &tag);

    QHash<QString, HistoryRow> history() const;
    bool recordOpen(const QString &key, const QDateTime &when = QDateTime::currentDateTimeUtc());
    bool clearHistory();

    // Drops history for the keys, and favorites and tags too when everything is true
    bool forget(const QStringList &keys, bool everything);

    // Moves all data from one path to another, used when a file was moved
    bool relocate(const QString &oldKey, const QString &newKey, const QString &newUri);

    QJsonObject exportJson() const;
    bool importJson(const QJsonObject &root, int *imported, QString *error);

    // Trims, collapses spaces and rejects empty or oversized tags
    static QString normalizeTag(const QString &tag);

private:
    bool migrate(QString *error);
    bool exec(const QString &sql, const QVariantList &values = {}) const;

    QString m_connection;
    QString m_file;
    QString m_recoveryNote;
    bool m_open = false;
    bool m_newerSchema = false;
};

} // namespace sheaf
