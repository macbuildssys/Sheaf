#pragma once

#include "database.h"
#include "types.h"

#include <QHash>
#include <QVector>

namespace sheaf {

struct ScanOptions {
    QString recentFile;
    QString kdeDir;
    bool includeKde = true;
    bool showMissing = true;
    bool readProgress = true;
    QStringList exclusions;
    int maxItems = 5000;
    bool autoDetectNetwork = true;
    QStringList networkMounts;
};

struct ScanInput {
    ScanOptions options;
    QVector<FavoriteRow> favorites;
    QHash<QString, QStringList> tags;
    QHash<QString, HistoryRow> history;
};

struct ScanResult {
    QVector<DocumentInfo> documents;
    QStringList warnings;
};

// Pure function with no shared state, safe to run on a worker thread
ScanResult scanDocuments(const ScanInput &input);

// The identity used to join a file across all sources
QString documentKey(const QString &path, const QStringList &networkMounts);

QStringList defaultExclusions();

} // namespace sheaf
