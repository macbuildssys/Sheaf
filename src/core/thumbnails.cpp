#include "thumbnails.h"

#include "paths.h"
#include "pathutil.h"

#include <sheaf/version.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace sheaf {
namespace thumbnails {

namespace {

const char *const kSizeDirs[] = {"large", "x-large", "xx-large", "normal"};
constexpr qint64 kMaxImageFileBytes = 256LL * 1024 * 1024;
constexpr qint64 kMaxImagePixels = 150LL * 1000 * 1000;

QMutex g_cacheMutex;
QVector<ThumbnailerSpec> g_cache;
bool g_cacheLoaded = false;

QString failDir()
{
    return paths::thumbnailRoot() + QStringLiteral("/fail/sheaf-") + QString::fromLatin1(SHEAF_VERSION);
}

bool markedFailed(const ThumbRequest &request)
{
    const QString file = failDir() + QLatin1Char('/') + cacheName(request.uri) + QStringLiteral(".png");
    if (!QFile::exists(file)) {
        return false;
    }
    const QImage marker(file);
    return marker.text(QStringLiteral("Thumb::MTime")) == QString::number(request.mtime);
}

void markFailed(const ThumbRequest &request)
{
    paths::ensurePrivateDir(failDir());
    QImage marker(1, 1, QImage::Format_ARGB32);
    marker.fill(Qt::transparent);
    marker.setText(QStringLiteral("Thumb::URI"), request.uri);
    marker.setText(QStringLiteral("Thumb::MTime"), QString::number(request.mtime));
    marker.save(failDir() + QLatin1Char('/') + cacheName(request.uri) + QStringLiteral(".png"), "PNG");
}

QImage fitTo(const QImage &image, int pixels)
{
    if (image.isNull()) {
        return image;
    }
    if (image.width() <= pixels && image.height() <= pixels) {
        return image;
    }
    return image.scaled(pixels, pixels, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

bool store(const QImage &image, const ThumbRequest &request)
{
    const QString dir = paths::thumbnailRoot() + QStringLiteral("/large");
    if (!paths::ensurePrivateDir(paths::thumbnailRoot()) || !paths::ensurePrivateDir(dir)) {
        return false;
    }
    QImage copy = image;
    copy.setText(QStringLiteral("Thumb::URI"), request.uri);
    copy.setText(QStringLiteral("Thumb::MTime"), QString::number(request.mtime));
    copy.setText(QStringLiteral("Software"), QStringLiteral("Sheaf %1").arg(QString::fromLatin1(SHEAF_VERSION)));
    const QString file = dir + QLatin1Char('/') + cacheName(request.uri) + QStringLiteral(".png");
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (!copy.save(&out, "PNG")) {
        out.cancelWriting();
        return false;
    }
    if (!out.commit()) {
        return false;
    }
    paths::makePrivate(file);
    return true;
}

} // namespace

QString cacheName(const QString &uri)
{
    return QString::fromLatin1(QCryptographicHash::hash(uri.toUtf8(), QCryptographicHash::Md5).toHex());
}

QString cachePath(const QString &uri, const QString &sizeDir)
{
    return paths::thumbnailRoot() + QLatin1Char('/') + sizeDir + QLatin1Char('/') + cacheName(uri) + QStringLiteral(".png");
}

QImage lookup(const ThumbRequest &request)
{
    QVector<QString> order;
    for (const char *dir : kSizeDirs) {
        order << QString::fromLatin1(dir);
    }
    // Prefer the smallest size that is still big enough
    const int wanted = request.pixels;
    auto sizeOf = [](const QString &dir) {
        if (dir == QLatin1String("normal")) {
            return 128;
        }
        if (dir == QLatin1String("large")) {
            return 256;
        }
        if (dir == QLatin1String("x-large")) {
            return 512;
        }
        return 1024;
    };
    std::sort(order.begin(), order.end(), [&](const QString &a, const QString &b) {
        const bool aOk = sizeOf(a) >= wanted;
        const bool bOk = sizeOf(b) >= wanted;
        if (aOk != bOk) {
            return aOk;
        }
        return aOk ? sizeOf(a) < sizeOf(b) : sizeOf(a) > sizeOf(b);
    });
    for (const QString &dir : order) {
        const QString file = cachePath(request.uri, dir);
        if (!QFile::exists(file)) {
            continue;
        }
        QImage image(file);
        if (image.isNull()) {
            continue;
        }
        const QString stamp = image.text(QStringLiteral("Thumb::MTime"));
        if (!stamp.isEmpty() && request.mtime != 0 && stamp != QString::number(request.mtime)) {
            continue;
        }
        return image;
    }
    return QImage();
}

QVector<ThumbnailerSpec> parseThumbnailerFile(const QString &file)
{
    QVector<ThumbnailerSpec> result;
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly) || in.size() > 64 * 1024) {
        return result;
    }
    ThumbnailerSpec spec;
    bool inEntry = false;
    const QList<QByteArray> lines = in.readAll().split('\n');
    for (const QByteArray &raw : lines) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inEntry = line == QLatin1String("[Thumbnailer Entry]");
            continue;
        }
        if (!inEntry || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if (key == QLatin1String("Exec")) {
            spec.exec = value;
        } else if (key == QLatin1String("TryExec")) {
            spec.tryExec = value;
        } else if (key == QLatin1String("MimeType")) {
            spec.mimeTypes = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
            for (QString &mime : spec.mimeTypes) {
                mime = mime.trimmed();
            }
        }
    }
    if (!spec.exec.isEmpty() && !spec.mimeTypes.isEmpty()) {
        result << spec;
    }
    return result;
}

QVector<ThumbnailerSpec> systemThumbnailers()
{
    QMutexLocker lock(&g_cacheMutex);
    if (g_cacheLoaded) {
        return g_cache;
    }
    g_cache.clear();
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("thumbnailers"),
                                                       QStandardPaths::LocateDirectory);
    for (const QString &dir : dirs) {
        const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("*.thumbnailer")}, QDir::Files | QDir::Readable);
        for (const QFileInfo &fi : files) {
            g_cache += parseThumbnailerFile(fi.absoluteFilePath());
        }
    }
    g_cacheLoaded = true;
    return g_cache;
}

void resetThumbnailerCache()
{
    QMutexLocker lock(&g_cacheMutex);
    g_cache.clear();
    g_cacheLoaded = false;
}

QStringList buildThumbnailerCommand(const QString &exec, const QString &uri, const QString &input, const QString &output, int size)
{
    QStringList args = QProcess::splitCommand(exec);
    if (args.isEmpty()) {
        return {};
    }
    for (QString &arg : args) {
        QString out;
        for (int i = 0; i < arg.size(); ++i) {
            if (arg[i] != QLatin1Char('%') || i + 1 >= arg.size()) {
                out.append(arg[i]);
                continue;
            }
            const QChar code = arg[++i];
            if (code == QLatin1Char('u')) {
                out += uri;
            } else if (code == QLatin1Char('i')) {
                out += input;
            } else if (code == QLatin1Char('o')) {
                out += output;
            } else if (code == QLatin1Char('s')) {
                out += QString::number(size);
            } else if (code == QLatin1Char('%')) {
                out += QLatin1Char('%');
            } else {
                return {};
            }
        }
        arg = out;
    }
    const QString program = QDir::isAbsolutePath(args.first()) ? args.first() : QStandardPaths::findExecutable(args.first());
    if (program.isEmpty()) {
        return {};
    }
    args[0] = program;
    return args;
}

static QImage generateWithQt(const ThumbRequest &request, QString *reason)
{
    const QFileInfo info(request.path);
    if (info.size() > kMaxImageFileBytes) {
        if (reason) {
            *reason = QStringLiteral("file too large");
        }
        return QImage();
    }
    QImageReader reader(request.path);
    reader.setAutoTransform(true);
    const QSize original = reader.size();
    if (original.isValid()) {
        if (static_cast<qint64>(original.width()) * original.height() > kMaxImagePixels) {
            if (reason) {
                *reason = QStringLiteral("image too large");
            }
            return QImage();
        }
        QSize target = original;
        target.scale(256, 256, Qt::KeepAspectRatio);
        if (target.isValid() && target.width() < original.width()) {
            reader.setScaledSize(target);
        }
    }
    const QImage image = reader.read();
    if (image.isNull() && reason) {
        *reason = reader.errorString();
    }
    return fitTo(image, 256);
}

static QImage generateWithThumbnailer(const ThumbRequest &request, QString *reason)
{
    const QVector<ThumbnailerSpec> all = systemThumbnailers();
    for (const ThumbnailerSpec &spec : all) {
        if (!spec.mimeTypes.contains(request.mime)) {
            continue;
        }
        if (!spec.tryExec.isEmpty() && QStandardPaths::findExecutable(spec.tryExec).isEmpty() && !QFileInfo(spec.tryExec).isExecutable()) {
            continue;
        }
        paths::ensurePrivateDir(paths::thumbnailRoot());
        const QString dir = paths::thumbnailRoot() + QStringLiteral("/large");
        paths::ensurePrivateDir(dir);
        const QString temp = dir + QStringLiteral("/.sheaf-") + QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".png");
        const QStringList command = buildThumbnailerCommand(spec.exec, request.uri, request.path, temp, 256);
        if (command.isEmpty()) {
            continue;
        }
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setStandardInputFile(QProcess::nullDevice());
        process.start(command.first(), command.mid(1));
        if (!process.waitForFinished(15000)) {
            process.kill();
            process.waitForFinished(2000);
            QFile::remove(temp);
            if (reason) {
                *reason = QStringLiteral("thumbnailer timed out");
            }
            return QImage();
        }
        QImage image;
        if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 && QFileInfo(temp).size() < 32 * 1024 * 1024) {
            image = QImage(temp);
        }
        QFile::remove(temp);
        if (!image.isNull()) {
            return fitTo(image, 256);
        }
        if (reason) {
            *reason = QStringLiteral("thumbnailer failed");
        }
    }
    return QImage();
}

QImage generate(const ThumbRequest &request, QString *reason)
{
    if (!request.allowGenerate || !QDir::isAbsolutePath(request.path)) {
        return QImage();
    }
    if (markedFailed(request)) {
        if (reason) {
            *reason = QStringLiteral("a previous attempt failed");
        }
        return QImage();
    }

    QImage image;
    if (request.mime.startsWith(QLatin1String("image/")) && QImageReader::supportedMimeTypes().contains(request.mime.toLatin1())) {
        image = generateWithQt(request, reason);
    }
    if (image.isNull()) {
        image = generateWithThumbnailer(request, reason);
    }
    if (image.isNull()) {
        markFailed(request);
        return QImage();
    }
    store(image, request);
    return image;
}

QImage load(const ThumbRequest &request)
{
    QImage image = lookup(request);
    if (!image.isNull()) {
        return image;
    }
    return generate(request);
}

} // namespace thumbnails
} // namespace sheaf
