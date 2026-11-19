#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace sheaf {

// One document as shown in the library, merged from every source
struct DocumentInfo {
    QString uri;
    QString path;
    QString key;
    QString name;
    QString mime;
    QString mimeComment;
    QString extension;
    QString haystack;
    QString title;
    QString author;
    qint64 size = 0;
    QDateTime modified;
    QDateTime lastUsed;
    QDateTime favoriteAdded;
    int openCount = 0;
    double progress = -1.0;
    bool exists = true;
    bool remote = false;
    bool favorite = false;
    bool inRecents = false;
    QStringList tags;
    QStringList apps;
    QStringList aliases;

    QString displayName() const { return title.isEmpty() ? name : title; }
    QString folder() const;
};

enum class SortMode {
    LastUsed,
    Name,
    Modified,
    Size,
    Type,
};

struct SortOptions {
    SortMode mode = SortMode::LastUsed;
    bool reversed = false;
    bool favoritesFirst = true;
};

QString sortModeToString(SortMode mode);
SortMode sortModeFromString(const QString &text, bool *ok = nullptr);

} // namespace sheaf

Q_DECLARE_METATYPE(sheaf::DocumentInfo)
