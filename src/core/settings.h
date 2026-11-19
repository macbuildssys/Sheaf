#pragma once

#include "types.h"

#include <QByteArray>
#include <QStringList>

namespace sheaf {

struct Settings {
    bool gridView = true;
    int zoom = 160;
    bool showDetails = false;
    SortOptions sort;
    QStringList exclusions;
    bool includeKde = true;
    bool showMissing = false;
    int maxItems = 5000;
    bool generateThumbnails = true;
    bool confirmTrash = true;
    QByteArray geometry;
    QByteArray splitterState;

    Settings();

    static Settings load(const QString &file);
    bool save(const QString &file) const;

    static constexpr int kMinZoom = 96;
    static constexpr int kMaxZoom = 320;
};

} // namespace sheaf
