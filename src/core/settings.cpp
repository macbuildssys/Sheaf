#include "settings.h"

#include "paths.h"
#include "scanner.h"

#include <QFileInfo>
#include <QSettings>

#include <algorithm>

namespace sheaf {

Settings::Settings()
    : exclusions(defaultExclusions())
{
}

Settings Settings::load(const QString &file)
{
    Settings s;
    if (!QFileInfo::exists(file)) {
        return s;
    }
    QSettings ini(file, QSettings::IniFormat);
    s.gridView = ini.value(QStringLiteral("view/grid"), s.gridView).toBool();
    s.zoom = std::clamp(ini.value(QStringLiteral("view/zoom"), s.zoom).toInt(), kMinZoom, kMaxZoom);
    s.showDetails = ini.value(QStringLiteral("view/details"), s.showDetails).toBool();
    bool ok = false;
    const SortMode mode = sortModeFromString(ini.value(QStringLiteral("sort/mode")).toString(), &ok);
    if (ok) {
        s.sort.mode = mode;
    }
    s.sort.reversed = ini.value(QStringLiteral("sort/reversed"), s.sort.reversed).toBool();
    s.sort.favoritesFirst = ini.value(QStringLiteral("sort/favoritesFirst"), s.sort.favoritesFirst).toBool();
    if (ini.contains(QStringLiteral("library/exclusions"))) {
        s.exclusions = ini.value(QStringLiteral("library/exclusions")).toStringList();
        s.exclusions.removeAll(QString());
    }
    s.includeKde = ini.value(QStringLiteral("library/includeKde"), s.includeKde).toBool();
    s.showMissing = ini.value(QStringLiteral("library/showMissingFiles"), s.showMissing).toBool();
    s.maxItems = std::clamp(ini.value(QStringLiteral("library/maxItems"), s.maxItems).toInt(), 50, 100000);
    s.generateThumbnails = ini.value(QStringLiteral("thumbnails/generate"), s.generateThumbnails).toBool();
    s.confirmTrash = ini.value(QStringLiteral("ui/confirmTrash"), s.confirmTrash).toBool();
    s.geometry = QByteArray::fromBase64(ini.value(QStringLiteral("window/geometry")).toByteArray());
    s.splitterState = QByteArray::fromBase64(ini.value(QStringLiteral("window/splitter")).toByteArray());
    return s;
}

bool Settings::save(const QString &file) const
{
    paths::ensurePrivateDir(QFileInfo(file).absolutePath());
    QSettings ini(file, QSettings::IniFormat);
    ini.setValue(QStringLiteral("view/grid"), gridView);
    ini.setValue(QStringLiteral("view/zoom"), zoom);
    ini.setValue(QStringLiteral("view/details"), showDetails);
    ini.setValue(QStringLiteral("sort/mode"), sortModeToString(sort.mode));
    ini.setValue(QStringLiteral("sort/reversed"), sort.reversed);
    ini.setValue(QStringLiteral("sort/favoritesFirst"), sort.favoritesFirst);
    ini.setValue(QStringLiteral("library/exclusions"), exclusions.isEmpty() ? QStringList{QString()} : exclusions);
    ini.setValue(QStringLiteral("library/includeKde"), includeKde);
    ini.setValue(QStringLiteral("library/showMissingFiles"), showMissing);
    ini.setValue(QStringLiteral("library/maxItems"), maxItems);
    ini.setValue(QStringLiteral("thumbnails/generate"), generateThumbnails);
    ini.setValue(QStringLiteral("ui/confirmTrash"), confirmTrash);
    ini.setValue(QStringLiteral("window/geometry"), QString::fromLatin1(geometry.toBase64()));
    ini.setValue(QStringLiteral("window/splitter"), QString::fromLatin1(splitterState.toBase64()));
    ini.sync();
    paths::makePrivate(file);
    return ini.status() == QSettings::NoError;
}

} // namespace sheaf
