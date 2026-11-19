#pragma once

#include <QString>
#include <QStringList>

namespace sheaf {

// Brings data over from Linux Mint's Thingy. Thingy keeps its favorites in the
// GSettings key org.x.apps.favorites and its groups in ~/.config/thingy/groups.json
namespace thingy {

QString groupsFile();

// Reads the output of "gsettings get org.x.apps.favorites list"
QStringList parseGsettingsFavorites(const QString &text);

// Asks gsettings for the favorites. Returns false when gsettings is not available
bool readFavorites(QStringList *uris, QString *error);

} // namespace thingy
} // namespace sheaf
