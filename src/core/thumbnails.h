#pragma once

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

namespace sheaf {

struct ThumbRequest {
    QString path;
    QString uri;
    QString mime;
    qint64 mtime = 0;
    int pixels = 256;
    bool allowGenerate = true;
};

struct ThumbnailerSpec {
    QStringList mimeTypes;
    QString exec;
    QString tryExec;
};

// Freedesktop thumbnail cache: reads thumbnails other apps made, and makes
// missing ones with Qt for images or with the system thumbnailer programs
namespace thumbnails {

QString cacheName(const QString &uri);
QString cachePath(const QString &uri, const QString &sizeDir);

QImage lookup(const ThumbRequest &request);
QImage generate(const ThumbRequest &request, QString *reason = nullptr);

// Lookup first, then generation when allowed
QImage load(const ThumbRequest &request);

QVector<ThumbnailerSpec> parseThumbnailerFile(const QString &file);
QVector<ThumbnailerSpec> systemThumbnailers();
void resetThumbnailerCache();

// Expands the placeholders of an Exec line into an argument list, empty when unsafe
QStringList buildThumbnailerCommand(const QString &exec, const QString &uri, const QString &input, const QString &output, int size);

} // namespace thumbnails
} // namespace sheaf
