#pragma once

#include <QString>

namespace sheaf {

enum class OpenResult {
    Ok,
    Missing,
    Blocked,
    Failed,
};

// Opening, revealing and trashing files. These never run through a shell
namespace launcher {

// Types that start a program instead of showing a document are never opened
bool isBlockedType(const QString &mime, const QString &path);

// Opens with the default application. SHEAF_OPENER can name another program
OpenResult open(const QString &path, const QString &mime);

// Shows the file in the file manager, asks over D-Bus first
bool showInFolder(const QString &path);

bool moveToTrash(const QString &path, QString *error = nullptr);

// The program that open() will start, exposed for tests
QString openerProgram();

} // namespace launcher
} // namespace sheaf
