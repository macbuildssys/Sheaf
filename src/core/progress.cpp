#include "progress.h"

#include <sheaf/version.h>

#if SHEAF_HAVE_GIO_CONFIG
// GIO has a struct member called signals, which Qt defines as a keyword
#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")
#endif

#include <algorithm>

namespace sheaf {

bool progressSupported()
{
    return SHEAF_HAVE_GIO_CONFIG != 0;
}

bool progressCandidate(const QString &mime)
{
    return mime == QLatin1String("application/pdf") || mime == QLatin1String("application/epub+zip") ||
           mime.startsWith(QLatin1String("image/vnd.djvu")) || mime.contains(QLatin1String("comicbook")) ||
           mime.startsWith(QLatin1String("application/x-cb")) || mime.contains(QLatin1String("mobi")) ||
           mime == QLatin1String("application/postscript") || mime.contains(QLatin1String("xps"));
}

ProgressInfo interpretProgress(const QString &page, const QString &pageCount, const QString &epubProgress, const QString &epubTitle,
                               const QString &epubAuthor)
{
    ProgressInfo info;
    info.title = epubTitle.left(300);
    info.author = epubAuthor.left(300);

    bool ok = false;
    if (!epubProgress.isEmpty()) {
        const int value = epubProgress.toInt(&ok);
        if (ok) {
            info.fraction = std::clamp(value / 100.0, 0.0, 1.0);
            return info;
        }
    }
    if (!page.isEmpty() && !pageCount.isEmpty()) {
        bool okPage = false;
        bool okCount = false;
        const int current = page.toInt(&okPage);
        const int count = pageCount.toInt(&okCount);
        // Short documents and unread ones carry no useful progress
        if (okPage && okCount && count > 4 && current > 0) {
            info.fraction = std::clamp(static_cast<double>(current) / (count - 1), 0.0, 1.0);
        }
    }
    return info;
}

#if SHEAF_HAVE_GIO_CONFIG

namespace {

QString attribute(GFileInfo *info, const char *name)
{
    const char *value = g_file_info_get_attribute_string(info, name);
    return value ? QString::fromUtf8(value) : QString();
}

} // namespace

ProgressInfo readReadingProgress(const QString &path)
{
    ProgressInfo result;
    const QByteArray native = path.toUtf8();
    GFile *file = g_file_new_for_path(native.constData());
    GError *error = nullptr;
    GFileInfo *info = g_file_query_info(file,
                                        "metadata::xreader::page,metadata::xreader::num-pages,metadata::xepub::progress,"
                                        "metadata::xepub::title,metadata::xepub::author",
                                        G_FILE_QUERY_INFO_NONE, nullptr, &error);
    if (info) {
        result = interpretProgress(attribute(info, "metadata::xreader::page"), attribute(info, "metadata::xreader::num-pages"),
                                   attribute(info, "metadata::xepub::progress"), attribute(info, "metadata::xepub::title"),
                                   attribute(info, "metadata::xepub::author"));
        g_object_unref(info);
    }
    if (error) {
        g_error_free(error);
    }
    g_object_unref(file);
    return result;
}

#else

ProgressInfo readReadingProgress(const QString &)
{
    return ProgressInfo();
}

#endif

} // namespace sheaf
