#include "guimain.h"

#include "mainwindow.h"
#include "singleinstance.h"

#include <sheaf/version.h>

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QTranslator>

namespace sheaf {
namespace gui {

static void loadTranslations(QApplication &app)
{
    const QLocale locale;
    auto *qt = new QTranslator(&app);
    const QString qtPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qt->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), qtPath)) {
        app.installTranslator(qt);
    }
    auto *own = new QTranslator(&app);
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("sheaf/translations"),
                                                       QStandardPaths::LocateDirectory);
    for (const QString &dir : dirs) {
        if (own->load(locale, QStringLiteral("sheaf"), QStringLiteral("_"), dir)) {
            app.installTranslator(own);
            break;
        }
    }
}

int run(int argc, char **argv, bool quick)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("sheaf"));
    app.setApplicationDisplayName(QStringLiteral("Sheaf"));
    app.setApplicationVersion(QString::fromLatin1(SHEAF_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("sheaf"));

    // Without a desktop theme Qt falls back to a very plain style
    if (app.style()->objectName().compare(QLatin1String("windows"), Qt::CaseInsensitive) == 0 && QStyleFactory::keys().contains(QStringLiteral("Fusion"))) {
        app.setStyle(QStringLiteral("Fusion"));
    }
    loadTranslations(app);

    SingleInstance single;
    if (single.sendToRunning(quick ? QStringLiteral("quick") : QStringLiteral("show"))) {
        return 0;
    }
    single.listen();

    MainWindow window;
    if (!window.isReady()) {
        return 1;
    }
    QObject::connect(&single, &SingleInstance::commandReceived, &window, [&window](const QString &command) {
        if (command == QLatin1String("quick")) {
            window.showQuickOpen();
        } else {
            window.setQuickOnly(false);
            window.raiseWindow();
        }
    });

    if (quick) {
        window.setQuickOnly(true);
        window.showQuickOpen();
    } else {
        window.show();
    }
    return app.exec();
}

} // namespace gui
} // namespace sheaf
