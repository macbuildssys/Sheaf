#include "groups.h"

#include "paths.h"
#include "pathutil.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QFileInfo>
#include <QUuid>

#include <utility>

namespace sheaf {

namespace {

struct BuiltinDef {
    const char *id;
    const char *title;
    const char *icons;
    const char *patterns;
    bool favoritesOnly;
    bool hiddenByDefault;
};

// The ids of the groups that Thingy also has are kept so its settings import cleanly.
// The id "documents" is the PDF group, as it is in Thingy
const BuiltinDef kBuiltins[] = {
    {"favorites", QT_TRANSLATE_NOOP("Groups", "Favorites"), "emblem-favorite,starred,bookmark-new", "*", true, false},
    {"all", QT_TRANSLATE_NOOP("Groups", "All Files"), "folder-documents,document-multiple,folder", "*", false, false},
    {"text-files", QT_TRANSLATE_NOOP("Groups", "Text Files"), "text-x-generic,accessories-text-editor",
     "text/plain,text/markdown,text/x-markdown", false, false},
    {"documents", QT_TRANSLATE_NOOP("Groups", "PDFs"), "application-pdf,x-office-document",
     "application/pdf,application/postscript,application/oxps,application/vnd.ms-xpsdocument,image/vnd.djvu,image/vnd.djvu+multipage",
     false, false},
    {"books", QT_TRANSLATE_NOOP("Groups", "Books"), "application-epub+zip,accessories-dictionary,text-x-generic",
     "application/epub+zip,application/x-mobipocket-ebook,application/vnd.amazon.mobi8-ebook,application/x-fictionbook+xml,"
     "application/x-sony-bbeb,application/vnd.comicbook+zip,application/vnd.comicbook-rar,application/x-cb7,application/x-cbt,"
     "application/x-cbr,application/x-cbz",
     false, false},
    {"writer-documents", QT_TRANSLATE_NOOP("Groups", "Office Documents"), "x-office-document,libreoffice-writer",
     "application/rtf,text/rtf,application/msword,"
     "application/vnd.openxmlformats-officedocument.wordprocessingml.document,"
     "application/vnd.openxmlformats-officedocument.wordprocessingml.template,"
     "application/vnd.ms-word.document.macroEnabled.12,application/vnd.oasis.opendocument.text,"
     "application/vnd.oasis.opendocument.text-template,application/vnd.oasis.opendocument.text-flat-xml",
     false, false},
    {"spreadsheets", QT_TRANSLATE_NOOP("Groups", "Spreadsheets"), "x-office-spreadsheet,libreoffice-calc",
     "text/csv,application/vnd.ms-excel,application/vnd.openxmlformats-officedocument.spreadsheetml.sheet,"
     "application/vnd.oasis.opendocument.spreadsheet",
     false, false},
    {"presentations", QT_TRANSLATE_NOOP("Groups", "Presentations"), "x-office-presentation,libreoffice-impress",
     "application/vnd.ms-powerpoint,application/vnd.openxmlformats-officedocument.presentationml.presentation,"
     "application/vnd.oasis.opendocument.presentation",
     false, true},
    {"drawings", QT_TRANSLATE_NOOP("Groups", "Drawings"), "x-office-drawing,libreoffice-draw",
     "application/vnd.oasis.opendocument.graphics,application/vnd.oasis.opendocument.graphics-template,"
     "application/vnd.oasis.opendocument.graphics-flat-xml,application/vnd.sun.xml.draw,"
     "application/vnd.sun.xml.draw.template,application/vnd.stardivision.draw",
     false, true},
    {"databases", QT_TRANSLATE_NOOP("Groups", "Databases"), "application-vnd.oasis.opendocument.database,server-database",
     "application/vnd.oasis.opendocument.database,application/vnd.sun.xml.base", false, true},
    {"pictures", QT_TRANSLATE_NOOP("Groups", "Pictures"), "image-x-generic",
     "image/jpeg,image/png,image/gif,image/webp,image/tiff,image/bmp,image/svg+xml,image/heif,image/heic,image/avif,"
     "image/x-xcf,image/x-canon-cr2,image/x-nikon-nef",
     false, false},
    {"audio", QT_TRANSLATE_NOOP("Groups", "Audio"), "audio-x-generic", "audio/*", false, false},
    {"videos", QT_TRANSLATE_NOOP("Groups", "Videos"), "video-x-generic", "video/*", false, false},
    {"code", QT_TRANSLATE_NOOP("Groups", "Code"), "text-x-script,text-x-source,text-x-generic",
     "text/x-*,text/html,text/css,application/json,application/xml,text/xml,application/javascript,application/x-shellscript,"
     "application/x-yaml,application/toml,application/sql",
     false, true},
    {"archives", QT_TRANSLATE_NOOP("Groups", "Archives"), "package-x-generic,application-x-archive",
     "application/zip,application/x-tar,application/gzip,application/x-compressed-tar,application/x-7z-compressed,"
     "application/vnd.rar,application/x-rar,application/zstd,application/x-xz,application/x-bzip2",
     false, true},
};

QStringList splitList(const char *text)
{
    return QString::fromLatin1(text).split(QLatin1Char(','), Qt::SkipEmptyParts);
}

Group fromDef(const BuiltinDef &def)
{
    Group group;
    group.id = QString::fromLatin1(def.id);
    group.title = QString::fromLatin1(def.title);
    group.iconNames = splitList(def.icons);
    group.patterns = splitList(def.patterns);
    group.builtin = true;
    group.favoritesOnly = def.favoritesOnly;
    group.hidden = def.hiddenByDefault;
    return group;
}

QStringList cleanStrings(const QJsonValue &value)
{
    QStringList list;
    const QJsonArray array = value.toArray();
    for (const QJsonValue &item : array) {
        if (item.isString() && !item.toString().isEmpty() && item.toString().size() < 512) {
            list << item.toString();
        }
    }
    return list;
}

} // namespace

QString Group::displayTitle() const
{
    if (!builtin) {
        return title;
    }
    return QCoreApplication::translate("Groups", title.toUtf8().constData());
}

bool mimeMatches(const QStringList &patterns, const QString &mime)
{
    if (mime.isEmpty()) {
        return false;
    }
    for (const QString &pattern : patterns) {
        if (pattern == mime || pattern == QLatin1String("*") || pattern == QLatin1String("*/*")) {
            return true;
        }
        if (pattern.endsWith(QLatin1Char('*')) && mime.startsWith(pattern.left(pattern.size() - 1))) {
            return true;
        }
    }
    return false;
}

bool groupMatches(const Group &group, const DocumentInfo &doc)
{
    if (group.favoritesOnly) {
        return doc.favorite;
    }
    if (!group.patterns.isEmpty() && !mimeMatches(group.patterns, doc.mime)) {
        return false;
    }
    if (!group.folder.isEmpty() && !pathutil::isUnder(doc.path, group.folder)) {
        return false;
    }
    for (const QString &tag : group.tags) {
        bool found = false;
        for (const QString &own : doc.tags) {
            if (own.compare(tag, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

int familyForMime(const QString &mime)
{
    if (mime == QLatin1String("application/pdf") || mime.startsWith(QLatin1String("application/postscript"))) {
        return 0;
    }
    if (mime.contains(QLatin1String("wordprocessing")) || mime.contains(QLatin1String("msword")) ||
        mime.contains(QLatin1String("opendocument.text")) || mime.contains(QLatin1String("rtf"))) {
        return 1;
    }
    if (mime.contains(QLatin1String("spreadsheet")) || mime.contains(QLatin1String("excel")) || mime == QLatin1String("text/csv")) {
        return 2;
    }
    if (mime.contains(QLatin1String("presentation")) || mime.contains(QLatin1String("powerpoint"))) {
        return 3;
    }
    if (mime.startsWith(QLatin1String("image/"))) {
        return 4;
    }
    if (mime.startsWith(QLatin1String("audio/"))) {
        return 5;
    }
    if (mime.startsWith(QLatin1String("video/"))) {
        return 6;
    }
    if (mime.contains(QLatin1String("epub")) || mime.contains(QLatin1String("mobi")) || mime.contains(QLatin1String("comicbook")) ||
        mime.contains(QLatin1String("fictionbook"))) {
        return 7;
    }
    if (mime.startsWith(QLatin1String("text/"))) {
        return 8;
    }
    return 9;
}

QVector<Group> GroupsConfig::builtinGroups()
{
    QVector<Group> list;
    for (const BuiltinDef &def : kBuiltins) {
        list.append(fromDef(def));
    }
    return list;
}

GroupsConfig GroupsConfig::defaults()
{
    GroupsConfig config;
    for (const Group &group : builtinGroups()) {
        config.m_order << group.id;
        if (group.hidden) {
            config.m_hidden << group.id;
        }
    }
    return config;
}

bool GroupsConfig::validCustomId(const QString &id)
{
    if (id.isEmpty() || id.size() > 64) {
        return false;
    }
    for (const QChar ch : id) {
        if (!(ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('-'))) {
            return false;
        }
    }
    return true;
}

bool GroupsConfig::parseCustom(const QJsonObject &object, Group *group)
{
    const QString id = object.value(QLatin1String("id")).toString();
    const QString name = object.value(QLatin1String("name")).toString().trimmed();
    if (!validCustomId(id) || name.isEmpty() || name.size() > 200) {
        return false;
    }
    group->id = QLatin1String("custom-") + id;
    group->title = name;
    const QString icon = object.value(QLatin1String("icon")).toString();
    group->iconNames = icon.isEmpty() ? QStringList{QStringLiteral("text-x-generic")} : QStringList{icon};
    group->patterns = cleanStrings(object.value(QLatin1String("mime_types")));
    group->tags = cleanStrings(object.value(QLatin1String("tags")));
    const QJsonValue path = object.value(QLatin1String("path"));
    group->folder = path.isString() ? pathutil::cleanAbsolute(path.toString()) : QString();
    group->builtin = false;
    return !group->patterns.isEmpty() || !group->tags.isEmpty() || !group->folder.isEmpty();
}

GroupsConfig GroupsConfig::load(const QString &file, QString *warning)
{
    GroupsConfig config = defaults();
    QFile in(file);
    if (!in.exists()) {
        return config;
    }
    if (in.size() > 4 * 1024 * 1024 || !in.open(QIODevice::ReadOnly)) {
        if (warning) {
            *warning = QStringLiteral("Could not read %1, using default groups").arg(file);
        }
        return config;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(in.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (warning) {
            *warning = QStringLiteral("The groups file is damaged (%1), using default groups").arg(parseError.errorString());
        }
        return config;
    }
    const QJsonObject root = doc.object();
    config.m_hidden = cleanStrings(root.value(QLatin1String("hidden_groups")));
    config.m_order = cleanStrings(root.value(QLatin1String("group_order")));
    const QJsonArray custom = root.value(QLatin1String("custom_groups")).toArray();
    for (const QJsonValue &value : custom) {
        Group group;
        if (value.isObject() && parseCustom(value.toObject(), &group)) {
            config.m_custom.append(group);
        }
    }
    const QString active = root.value(QLatin1String("active_group")).toString();
    if (!active.isEmpty()) {
        config.m_active = active;
    }
    config.normalize();
    return config;
}

void GroupsConfig::normalize()
{
    QStringList valid;
    for (const Group &group : builtinGroups()) {
        valid << group.id;
    }
    for (const Group &group : m_custom) {
        valid << group.id;
    }

    QStringList order;
    for (const QString &id : std::as_const(m_order)) {
        if (valid.contains(id) && !order.contains(id)) {
            order << id;
        }
    }
    for (const QString &id : std::as_const(valid)) {
        if (!order.contains(id)) {
            order << id;
        }
    }
    m_order = order;

    QStringList hidden;
    for (const QString &id : std::as_const(m_hidden)) {
        if (valid.contains(id) && !hidden.contains(id)) {
            hidden << id;
        }
    }
    m_hidden = hidden;

    // Favorites can be reordered but must stay reachable
    m_hidden.removeAll(QStringLiteral("favorites"));
    m_hidden.removeAll(QStringLiteral("all"));
    if (!valid.contains(m_active)) {
        m_active = QStringLiteral("all");
    }
}

QVector<Group> GroupsConfig::allGroups() const
{
    QVector<Group> pool = builtinGroups();
    pool += m_custom;
    QVector<Group> result;
    for (const QString &id : m_order) {
        for (Group group : std::as_const(pool)) {
            if (group.id == id) {
                group.hidden = m_hidden.contains(id);
                result.append(group);
                break;
            }
        }
    }
    return result;
}

QVector<Group> GroupsConfig::visibleGroups() const
{
    QVector<Group> result;
    const QVector<Group> all = allGroups();
    for (const Group &group : all) {
        if (!group.hidden) {
            result.append(group);
        }
    }
    return result;
}

Group GroupsConfig::groupById(const QString &id, bool *found) const
{
    const QVector<Group> all = allGroups();
    for (const Group &group : all) {
        if (group.id == id) {
            if (found) {
                *found = true;
            }
            return group;
        }
    }
    if (found) {
        *found = false;
    }
    return Group();
}

QString GroupsConfig::addCustom(Group group)
{
    const QString raw = QUuid::createUuid().toString(QUuid::Id128);
    group.id = QLatin1String("custom-") + raw;
    group.builtin = false;
    group.favoritesOnly = false;
    m_custom.append(group);
    m_order << group.id;
    return group.id;
}

bool GroupsConfig::updateCustom(const Group &group)
{
    for (Group &existing : m_custom) {
        if (existing.id == group.id) {
            existing = group;
            existing.builtin = false;
            return true;
        }
    }
    return false;
}

bool GroupsConfig::removeCustom(const QString &id)
{
    for (int i = 0; i < m_custom.size(); ++i) {
        if (m_custom.at(i).id == id) {
            m_custom.removeAt(i);
            m_order.removeAll(id);
            m_hidden.removeAll(id);
            if (m_active == id) {
                m_active = QStringLiteral("all");
            }
            return true;
        }
    }
    return false;
}

void GroupsConfig::setHidden(const QString &id, bool hidden)
{
    if (id == QLatin1String("favorites") || id == QLatin1String("all")) {
        return;
    }
    m_hidden.removeAll(id);
    if (hidden) {
        m_hidden << id;
    }
}

void GroupsConfig::move(const QString &id, int delta)
{
    const int index = m_order.indexOf(id);
    const int target = index + delta;
    if (index < 0 || target < 0 || target >= m_order.size()) {
        return;
    }
    m_order.swapItemsAt(index, target);
}

void GroupsConfig::resetToDefaults()
{
    const QString active = m_active;
    *this = defaults();
    m_active = active;
    normalize();
}

bool GroupsConfig::save(const QString &file, QString *error) const
{
    QJsonArray custom;
    for (const Group &group : m_custom) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), group.id.mid(QStringLiteral("custom-").size()));
        object.insert(QStringLiteral("name"), group.title);
        object.insert(QStringLiteral("icon"), group.iconName());
        object.insert(QStringLiteral("mime_types"), QJsonArray::fromStringList(group.patterns));
        object.insert(QStringLiteral("tags"), QJsonArray::fromStringList(group.tags));
        object.insert(QStringLiteral("path"), group.folder.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(group.folder));
        custom.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("hidden_groups"), QJsonArray::fromStringList(m_hidden));
    root.insert(QStringLiteral("custom_groups"), custom);
    root.insert(QStringLiteral("group_order"), QJsonArray::fromStringList(m_order));
    root.insert(QStringLiteral("active_group"), m_active);

    paths::ensurePrivateDir(QFileInfo(file).absolutePath());
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = out.errorString();
        }
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        if (error) {
            *error = out.errorString();
        }
        return false;
    }
    paths::makePrivate(file);
    return true;
}

int GroupsConfig::importThingy(const QString &file, QString *error)
{
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly) || in.size() > 4 * 1024 * 1024) {
        if (error) {
            *error = QStringLiteral("Could not read %1").arg(file);
        }
        return -1;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(in.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = parseError.errorString();
        }
        return -1;
    }
    const QJsonObject root = doc.object();
    int imported = 0;
    const QJsonArray custom = root.value(QLatin1String("custom_groups")).toArray();
    for (const QJsonValue &value : custom) {
        Group group;
        if (!value.isObject() || !parseCustom(value.toObject(), &group)) {
            continue;
        }
        bool exists = false;
        for (const Group &known : std::as_const(m_custom)) {
            if (known.id == group.id) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_custom.append(group);
            m_order << group.id;
            ++imported;
        }
    }
    static const char *const thingyIds[] = {"favorites", "text-files", "documents", "books", "writer-documents", "spreadsheets",
                                            "presentations", "drawings", "databases", "pictures", "audio", "videos"};
    const QStringList hidden = cleanStrings(root.value(QLatin1String("hidden_groups")));
    for (const char *name : thingyIds) {
        const QString id = QString::fromLatin1(name);
        setHidden(id, hidden.contains(id));
    }
    normalize();
    return imported;
}

} // namespace sheaf
