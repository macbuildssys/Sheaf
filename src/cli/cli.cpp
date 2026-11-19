#include "cli.h"

#include "database.h"
#include "groups.h"
#include "index.h"
#include "launcher.h"
#include "paths.h"
#include "pathutil.h"
#include "scanner.h"
#include "search.h"
#include "settings.h"
#include "sorting.h"
#include "thingyimport.h"
#include "xbel.h"

#include <sheaf/version.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>

namespace sheaf {
namespace cli {

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

const char *const kCommands[] = {"list",   "search", "open",         "favorite", "tag",    "forget", "clear-recent", "clean",
                                 "groups", "paths",  "import-thingy", "export",   "import", "help",   "version"};

struct Env {
    Database db;
    Settings settings;
    GroupsConfig groups;
    QStringList mounts;

    bool init()
    {
        QString error;
        if (!db.open(paths::databaseFile(), &error)) {
            err() << "sheaf: " << error << "\n";
            return false;
        }
        if (!db.recoveryNote().isEmpty()) {
            err() << "sheaf: " << db.recoveryNote() << "\n";
        }
        settings = Settings::load(paths::settingsFile());
        groups = GroupsConfig::load(paths::groupsFile());
        mounts = pathutil::networkMountPoints();
        return true;
    }

    ScanOptions scanOptions() const
    {
        ScanOptions o;
        o.recentFile = paths::recentFile();
        o.kdeDir = paths::kdeRecentDir();
        o.includeKde = settings.includeKde;
        // Missing files are always scanned so clean can find them, list hides them below
        o.showMissing = true;
        o.exclusions = settings.exclusions;
        o.maxItems = settings.maxItems;
        o.networkMounts = mounts;
        return o;
    }

    QVector<DocumentInfo> scan() const
    {
        ScanInput input;
        input.options = scanOptions();
        input.favorites = db.favorites();
        input.tags = db.tags();
        input.history = db.history();
        const ScanResult result = scanDocuments(input);
        for (const QString &warning : result.warnings) {
            err() << "sheaf: " << warning << "\n";
        }
        return result.documents;
    }

    QString keyFor(const QString &path) const { return documentKey(path, mounts); }
};

QJsonObject toJson(const DocumentInfo &doc)
{
    QJsonObject o;
    o.insert(QStringLiteral("path"), doc.path);
    o.insert(QStringLiteral("name"), doc.displayName());
    o.insert(QStringLiteral("mime"), doc.mime);
    o.insert(QStringLiteral("type"), doc.mimeComment);
    o.insert(QStringLiteral("size"), static_cast<double>(doc.size));
    o.insert(QStringLiteral("modified"), doc.modified.isValid() ? doc.modified.toUTC().toString(Qt::ISODate) : QString());
    o.insert(QStringLiteral("last_used"), doc.lastUsed.isValid() ? doc.lastUsed.toUTC().toString(Qt::ISODate) : QString());
    o.insert(QStringLiteral("favorite"), doc.favorite);
    o.insert(QStringLiteral("missing"), !doc.exists);
    o.insert(QStringLiteral("opened_with_sheaf"), doc.openCount);
    o.insert(QStringLiteral("tags"), QJsonArray::fromStringList(doc.tags));
    if (doc.progress >= 0.0) {
        o.insert(QStringLiteral("progress"), doc.progress);
    }
    return o;
}

QString humanSize(qint64 bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return unit == 0 ? QStringLiteral("%1 B").arg(bytes) : QStringLiteral("%1 %2").arg(value, 0, 'f', 1).arg(QLatin1String(units[unit]));
}

// Parses the shared options and the positional arguments of a sub command
void setupParser(QCommandLineParser &parser, const QString &description)
{
    parser.setApplicationDescription(description);
    parser.addHelpOption();
}

int cmdList(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("List files. Words and operators (tag:x type:pdf in:folder is:fav is:missing -word) filter the list."));
    QCommandLineOption group({QStringLiteral("g"), QStringLiteral("group")}, QStringLiteral("Only files of this group id (see: sheaf groups)"), QStringLiteral("id"));
    QCommandLineOption tag({QStringLiteral("t"), QStringLiteral("tag")}, QStringLiteral("Only files with this tag, can be repeated"), QStringLiteral("tag"));
    QCommandLineOption fav({QStringLiteral("f"), QStringLiteral("favorites")}, QStringLiteral("Only favorites"));
    QCommandLineOption missing({QStringLiteral("m"), QStringLiteral("missing")}, QStringLiteral("Only files that were moved or deleted"));
    QCommandLineOption sort({QStringLiteral("s"), QStringLiteral("sort")}, QStringLiteral("Sort by used, name, modified, size or type"), QStringLiteral("mode"), QStringLiteral("used"));
    QCommandLineOption reverse({QStringLiteral("r"), QStringLiteral("reverse")}, QStringLiteral("Reverse the order"));
    QCommandLineOption limit({QStringLiteral("n"), QStringLiteral("limit")}, QStringLiteral("Show at most this many files"), QStringLiteral("count"));
    QCommandLineOption json({QStringLiteral("j"), QStringLiteral("json")}, QStringLiteral("Print JSON"));
    QCommandLineOption longFormat({QStringLiteral("l"), QStringLiteral("long")}, QStringLiteral("Print details, not just paths"));
    QCommandLineOption nul({QStringLiteral("0"), QStringLiteral("null")}, QStringLiteral("Separate paths with a NUL byte, for xargs -0"));
    parser.addOptions({group, tag, fav, missing, sort, reverse, limit, json, longFormat, nul});
    parser.addPositionalArgument(QStringLiteral("query"), QStringLiteral("Search words"), QStringLiteral("[query...]"));
    parser.process(args);

    bool sortOk = false;
    const SortMode mode = sortModeFromString(parser.value(sort), &sortOk);
    if (!sortOk) {
        err() << "sheaf: unknown sort mode '" << parser.value(sort) << "'\n";
        return 2;
    }
    int max = -1;
    if (parser.isSet(limit)) {
        bool ok = false;
        max = parser.value(limit).toInt(&ok);
        if (!ok || max < 0) {
            err() << "sheaf: --limit needs a number\n";
            return 2;
        }
    }

    Env env;
    if (!env.init()) {
        return 1;
    }
    Group wanted;
    bool haveGroup = false;
    if (parser.isSet(group)) {
        wanted = env.groups.groupById(parser.value(group), &haveGroup);
        if (!haveGroup) {
            err() << "sheaf: unknown group '" << parser.value(group) << "'\n";
            return 2;
        }
    }
    SearchQuery query = SearchQuery::parse(parser.positionalArguments().join(QLatin1Char(' ')));
    for (const QString &t : parser.values(tag)) {
        query.tags << foldForSearch(t);
    }
    query.onlyFavorites = query.onlyFavorites || parser.isSet(fav);
    query.onlyMissing = query.onlyMissing || parser.isSet(missing);

    QVector<DocumentInfo> docs;
    const QVector<DocumentInfo> all = env.scan();
    for (const DocumentInfo &doc : all) {
        if (haveGroup && !groupMatches(wanted, doc)) {
            continue;
        }
        if (!doc.exists && !env.settings.showMissing && !query.onlyMissing) {
            continue;
        }
        if (query.matches(doc)) {
            docs.append(doc);
        }
    }
    SortOptions sortOptions;
    sortOptions.mode = mode;
    sortOptions.reversed = parser.isSet(reverse);
    sortOptions.favoritesFirst = false;
    std::stable_sort(docs.begin(), docs.end(), [&](const DocumentInfo &x, const DocumentInfo &y) {
        return documentLess(x, y, sortOptions);
    });
    if (max >= 0 && docs.size() > max) {
        docs.resize(max);
    }

    if (parser.isSet(json)) {
        QJsonArray array;
        for (const DocumentInfo &doc : std::as_const(docs)) {
            array.append(toJson(doc));
        }
        out() << QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Indented));
        return 0;
    }
    for (const DocumentInfo &doc : std::as_const(docs)) {
        if (parser.isSet(longFormat)) {
            out() << (doc.favorite ? "*" : " ") << (doc.exists ? " " : "!") << " "
                  << (doc.modified.isValid() ? doc.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("                ")) << "  "
                  << humanSize(doc.size).rightJustified(9) << "  " << doc.path;
            if (!doc.tags.isEmpty()) {
                out() << "  [" << doc.tags.join(QStringLiteral(", ")) << "]";
            }
            out() << "\n";
        } else if (parser.isSet(nul)) {
            out() << doc.path << QChar(0);
        } else {
            out() << doc.path << "\n";
        }
    }
    return 0;
}

QStringList absolutePaths(const QStringList &args)
{
    QStringList paths;
    for (const QString &arg : args) {
        paths << pathutil::cleanAbsolute(arg);
    }
    return paths;
}

int cmdOpen(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("Open files with the default application"));
    parser.addPositionalArgument(QStringLiteral("path"), QStringLiteral("Files to open"), QStringLiteral("path..."));
    parser.process(args);
    if (parser.positionalArguments().isEmpty()) {
        err() << "sheaf open: give at least one file\n";
        return 2;
    }
    Env env;
    if (!env.init()) {
        return 1;
    }
    int status = 0;
    QMimeDatabase mimeDb;
    for (const QString &path : absolutePaths(parser.positionalArguments())) {
        const QString mime = mimeDb.mimeTypeForFile(path).name();
        switch (launcher::open(path, mime)) {
        case OpenResult::Ok:
            env.db.recordOpen(env.keyFor(path));
            break;
        case OpenResult::Missing:
            err() << "sheaf: " << path << " does not exist\n";
            status = 1;
            break;
        case OpenResult::Blocked:
            err() << "sheaf: " << path << " is a program, Sheaf only opens files\n";
            status = 1;
            break;
        case OpenResult::Failed:
            err() << "sheaf: could not start the opener for " << path << "\n";
            status = 1;
            break;
        }
    }
    return status;
}

int cmdFavorite(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("Manage favorites. Actions: add, remove, list"));
    parser.addPositionalArgument(QStringLiteral("action"), QStringLiteral("add, remove or list"));
    parser.addPositionalArgument(QStringLiteral("path"), QStringLiteral("Files"), QStringLiteral("[path...]"));
    parser.process(args);
    const QStringList pos = parser.positionalArguments();
    if (pos.isEmpty()) {
        err() << "sheaf favorite: give an action (add, remove or list)\n";
        return 2;
    }
    if (pos.first() == QLatin1String("list")) {
        return cmdList({QStringLiteral("sheaf list"), QStringLiteral("--favorites")});
    }
    const bool add = pos.first() == QLatin1String("add");
    if (!add && pos.first() != QLatin1String("remove")) {
        err() << "sheaf favorite: unknown action '" << pos.first() << "'\n";
        return 2;
    }
    if (pos.size() < 2) {
        err() << "sheaf favorite: give at least one file\n";
        return 2;
    }
    Env env;
    if (!env.init()) {
        return 1;
    }
    int status = 0;
    for (const QString &path : absolutePaths(pos.mid(1))) {
        if (add && !QFileInfo(path).isFile()) {
            err() << "sheaf: " << path << " is not a file\n";
            status = 1;
            continue;
        }
        const QString key = env.keyFor(path);
        if (!env.db.setFavorite(key, pathutil::uriFromPath(key), add)) {
            err() << "sheaf: could not update " << path << "\n";
            status = 1;
        }
    }
    return status;
}

int cmdTag(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("Manage tags. Actions: add TAG PATH..., remove TAG PATH..., list"));
    parser.addPositionalArgument(QStringLiteral("action"), QStringLiteral("add, remove or list"));
    parser.addPositionalArgument(QStringLiteral("arguments"), QStringLiteral("Tag and files"), QStringLiteral("[tag] [path...]"));
    parser.process(args);
    const QStringList pos = parser.positionalArguments();
    if (pos.isEmpty()) {
        err() << "sheaf tag: give an action (add, remove or list)\n";
        return 2;
    }
    Env env;
    if (!env.init()) {
        return 1;
    }
    if (pos.first() == QLatin1String("list")) {
        const QStringList names = env.db.allTagNames();
        for (const QString &name : names) {
            out() << name << "\n";
        }
        return 0;
    }
    const bool add = pos.first() == QLatin1String("add");
    if ((!add && pos.first() != QLatin1String("remove")) || pos.size() < 3) {
        err() << "sheaf tag: usage is 'tag add|remove TAG PATH...'\n";
        return 2;
    }
    if (add && Database::normalizeTag(pos.at(1)).isEmpty()) {
        err() << "sheaf tag: a tag needs 1 to 64 characters and no commas\n";
        return 2;
    }
    int status = 0;
    for (const QString &path : absolutePaths(pos.mid(2))) {
        if (add && !QFileInfo(path).isFile()) {
            err() << "sheaf: " << path << " is not a file\n";
            status = 1;
            continue;
        }
        const QString key = env.keyFor(path);
        const bool ok = add ? env.db.addTag(key, pos.at(1)) : env.db.removeTag(key, pos.at(1));
        if (!ok) {
            status = 1;
        }
    }
    return status;
}

int cmdForget(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("Remove files from the recent list. The files are not touched."));
    parser.addPositionalArgument(QStringLiteral("path"), QStringLiteral("Files"), QStringLiteral("path..."));
    parser.process(args);
    if (parser.positionalArguments().isEmpty()) {
        err() << "sheaf forget: give at least one file\n";
        return 2;
    }
    Env env;
    if (!env.init()) {
        return 1;
    }
    QSet<QString> set;
    QStringList keys;
    for (const QString &path : absolutePaths(parser.positionalArguments())) {
        set.insert(path);
        const QString key = env.keyFor(path);
        set.insert(key);
        keys << key;
    }
    QString error;
    int removed = 0;
    bool ok = xbel::removePaths(paths::recentFile(), set, &removed, &error);
    ok = env.db.forget(keys, false) && ok;
    if (!ok) {
        err() << "sheaf: " << error << "\n";
        return 1;
    }
    out() << removed << " removed from the recent list\n";
    return 0;
}

int cmdClearRecent(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("Clear the recent files list for all applications"));
    QCommandLineOption yes({QStringLiteral("y"), QStringLiteral("yes")}, QStringLiteral("Confirm, nothing happens without it"));
    parser.addOption(yes);
    parser.process(args);
    if (!parser.isSet(yes)) {
        err() << "sheaf clear-recent: this clears the history of every application, run again with --yes to confirm\n";
        return 2;
    }
    Env env;
    if (!env.init()) {
        return 1;
    }
    QString error;
    if (!xbel::clear(paths::recentFile(), &error) || !env.db.clearHistory()) {
        err() << "sheaf: could not clear the list: " << error << "\n";
        return 1;
    }
    out() << "Recent files cleared\n";
    return 0;
}

int cmdClean(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("Forget files that were moved or deleted"));
    QCommandLineOption dry({QStringLiteral("n"), QStringLiteral("dry-run")}, QStringLiteral("Only list what would be removed"));
    parser.addOption(dry);
    parser.process(args);
    Env env;
    if (!env.init()) {
        return 1;
    }
    QSet<QString> set;
    QStringList keys;
    int count = 0;
    const QVector<DocumentInfo> all = env.scan();
    for (const DocumentInfo &doc : all) {
        if (doc.exists) {
            continue;
        }
        ++count;
        out() << doc.path << "\n";
        set.insert(doc.path);
        set.insert(pathutil::pathFromUri(doc.uri));
        for (const QString &alias : doc.aliases) {
            set.insert(alias);
        }
        keys << doc.key;
    }
    if (parser.isSet(dry) || count == 0) {
        return 0;
    }
    QString error;
    int removed = 0;
    bool ok = xbel::removePaths(paths::recentFile(), set, &removed, &error);
    ok = env.db.forget(keys, true) && ok;
    if (!ok) {
        err() << "sheaf: " << error << "\n";
        return 1;
    }
    out() << count << " moved or deleted files forgotten\n";
    return 0;
}

int cmdGroups(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("List groups and their ids"));
    parser.process(args);
    const GroupsConfig config = GroupsConfig::load(paths::groupsFile());
    for (const Group &group : config.allGroups()) {
        out() << group.id.leftJustified(28) << (group.hidden ? "hidden   " : "visible  ") << group.title << "\n";
    }
    return 0;
}

int cmdPaths(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("Show where Sheaf keeps its data"));
    parser.process(args);
    out() << "database:   " << paths::databaseFile() << "\n"
          << "settings:   " << paths::settingsFile() << "\n"
          << "groups:     " << paths::groupsFile() << "\n"
          << "recent:     " << paths::recentFile() << "\n"
          << "kde recent: " << paths::kdeRecentDir() << "\n"
          << "thumbnails: " << paths::thumbnailRoot() << "\n";
    return 0;
}

int cmdImportThingy(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("Import favorites and groups from Linux Mint's Thingy"));
    QCommandLineOption noFav(QStringLiteral("no-favorites"), QStringLiteral("Skip the favorites"));
    QCommandLineOption noGroups(QStringLiteral("no-groups"), QStringLiteral("Skip the groups"));
    parser.addOptions({noFav, noGroups});
    parser.process(args);
    Env env;
    if (!env.init()) {
        return 1;
    }
    int favorites = 0;
    if (!parser.isSet(noFav)) {
        QStringList uris;
        QString error;
        if (thingy::readFavorites(&uris, &error)) {
            for (const QString &uri : uris) {
                const QString path = pathutil::pathFromUri(uri);
                if (path.isEmpty() || !QFileInfo(path).isFile()) {
                    continue;
                }
                const QString key = env.keyFor(path);
                if (env.db.setFavorite(key, uri, true)) {
                    ++favorites;
                }
            }
        } else {
            err() << "sheaf: favorites skipped: " << error << "\n";
        }
    }
    int groups = 0;
    if (!parser.isSet(noGroups) && QFileInfo::exists(thingy::groupsFile())) {
        GroupsConfig config = GroupsConfig::load(paths::groupsFile());
        QString error;
        groups = config.importThingy(thingy::groupsFile(), &error);
        if (groups < 0) {
            err() << "sheaf: groups skipped: " << error << "\n";
            groups = 0;
        } else if (!config.save(paths::groupsFile(), &error)) {
            err() << "sheaf: could not save groups: " << error << "\n";
            return 1;
        }
    }
    out() << favorites << " favorites and " << groups << " custom groups imported\n";
    return 0;
}

int cmdExport(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("Write favorites and tags to a JSON file"));
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Output file"));
    parser.process(args);
    if (parser.positionalArguments().size() != 1) {
        err() << "sheaf export: give one output file\n";
        return 2;
    }
    Env env;
    if (!env.init()) {
        return 1;
    }
    QSaveFile file(parser.positionalArguments().first());
    if (!file.open(QIODevice::WriteOnly)) {
        err() << "sheaf: " << file.errorString() << "\n";
        return 1;
    }
    file.write(QJsonDocument(env.db.exportJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        err() << "sheaf: " << file.errorString() << "\n";
        return 1;
    }
    return 0;
}

int cmdImport(const QStringList &args)
{
    QCommandLineParser parser;
    setupParser(parser, QStringLiteral("Read favorites and tags from a file made by sheaf export"));
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Input file"));
    parser.process(args);
    if (parser.positionalArguments().size() != 1) {
        err() << "sheaf import: give one input file\n";
        return 2;
    }
    QFile file(parser.positionalArguments().first());
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024 * 1024) {
        err() << "sheaf: could not read " << file.fileName() << "\n";
        return 1;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        err() << "sheaf: not a valid export file\n";
        return 1;
    }
    Env env;
    if (!env.init()) {
        return 1;
    }
    int count = 0;
    QString error;
    if (!env.db.importJson(doc.object(), &count, &error)) {
        err() << "sheaf: " << error << "\n";
        return 1;
    }
    out() << count << " items imported\n";
    return 0;
}

} // namespace

bool isCommand(const QString &word)
{
    for (const char *name : kCommands) {
        if (word == QLatin1String(name)) {
            return true;
        }
    }
    return false;
}

QString helpText()
{
    return QStringLiteral(
        "Usage: sheaf [command] [options]\n"
        "\n"
        "With no command Sheaf opens its window.\n"
        "\n"
        "Commands:\n"
        "  list [query]        List files (alias: search)\n"
        "  open PATH...        Open files with the default application\n"
        "  favorite add|remove|list [PATH...]\n"
        "  tag add|remove TAG PATH...   or   tag list\n"
        "  forget PATH...      Remove documents from the recent list\n"
        "  clear-recent --yes  Clear the recent list for every application\n"
        "  clean [--dry-run]   Forget files that were moved or deleted\n"
        "  groups              List groups and their ids\n"
        "  paths               Show where Sheaf keeps its data\n"
        "  import-thingy       Import favorites and groups from Thingy\n"
        "  export FILE         Save favorites and tags to a JSON file\n"
        "  import FILE         Restore favorites and tags from a JSON file\n"
        "  version             Show the version\n"
        "\n"
        "Window options:\n"
        "  --quick             Open the quick search window (starts Sheaf if needed)\n"
        "  -h, --help          Show this help\n"
        "  -V, --version       Show the version\n"
        "\n"
        "Run 'sheaf COMMAND --help' for the options of a command.\n");
}

int run(const QStringList &args)
{
    if (args.isEmpty()) {
        out() << helpText();
        return 0;
    }
    const QString command = args.first();
    // QCommandLineParser expects the program name first
    const QStringList rest = QStringList{QStringLiteral("sheaf ") + command} + args.mid(1);

    if (command == QLatin1String("help")) {
        out() << helpText();
        return 0;
    }
    if (command == QLatin1String("version")) {
        out() << "sheaf " << SHEAF_VERSION << "\n";
        return 0;
    }
    if (command == QLatin1String("list") || command == QLatin1String("search")) {
        return cmdList(rest);
    }
    if (command == QLatin1String("open")) {
        return cmdOpen(rest);
    }
    if (command == QLatin1String("favorite")) {
        return cmdFavorite(rest);
    }
    if (command == QLatin1String("tag")) {
        return cmdTag(rest);
    }
    if (command == QLatin1String("forget")) {
        return cmdForget(rest);
    }
    if (command == QLatin1String("clear-recent")) {
        return cmdClearRecent(rest);
    }
    if (command == QLatin1String("clean")) {
        return cmdClean(rest);
    }
    if (command == QLatin1String("groups")) {
        return cmdGroups(rest);
    }
    if (command == QLatin1String("paths")) {
        return cmdPaths(rest);
    }
    if (command == QLatin1String("import-thingy")) {
        return cmdImportThingy(rest);
    }
    if (command == QLatin1String("export")) {
        return cmdExport(rest);
    }
    if (command == QLatin1String("import")) {
        return cmdImport(rest);
    }
    err() << "sheaf: unknown command '" << command << "'\n" << helpText();
    return 2;
}

} // namespace cli
} // namespace sheaf
