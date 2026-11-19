#include "cli.h"
#include "guimain.h"

#include <sheaf/version.h>

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QTextStream>

int main(int argc, char **argv)
{
    QStringList args;
    for (int i = 1; i < argc; ++i) {
        args << QString::fromLocal8Bit(argv[i]);
    }

    QTextStream out(stdout);
    if (!args.isEmpty() && sheaf::cli::isCommand(args.first())) {
        QCoreApplication app(argc, argv);
        return sheaf::cli::run(args);
    }
    if (args.contains(QStringLiteral("--help")) || args.contains(QStringLiteral("-h"))) {
        out << sheaf::cli::helpText();
        return 0;
    }
    if (args.contains(QStringLiteral("--version")) || args.contains(QStringLiteral("-V"))) {
        out << "sheaf " << SHEAF_VERSION << "\n";
        return 0;
    }
    bool quick = false;
    for (const QString &arg : args) {
        if (arg == QLatin1String("--quick")) {
            quick = true;
        } else if (arg.startsWith(QLatin1String("--")) && !arg.startsWith(QLatin1String("--style"))) {
            // Single dash options such as -platform belong to Qt
            QTextStream err(stderr);
            err << "sheaf: unknown option '" << arg << "'\n\n" << sheaf::cli::helpText();
            return 2;
        }
    }
    return sheaf::gui::run(argc, argv, quick);
}
