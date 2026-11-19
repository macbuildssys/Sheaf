#pragma once

#include <QStringList>

namespace sheaf {
namespace cli {

// True when the first argument names a command line sub command
bool isCommand(const QString &word);

// Runs a sub command without any window. args excludes the program name
int run(const QStringList &args);

QString helpText();

} // namespace cli
} // namespace sheaf
