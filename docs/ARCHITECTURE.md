# Architecture

Sheaf has three layers. Each one only knows about the layers below it.

## Core library (src/core)

A static library with no widgets, which makes it fast to test.

* types, paths and pathutil hold the shared data types, the standard folder locations and helpers for turning file URIs into paths and back. pathutil also reads `/proc/self/mountinfo` to find network mounts.
* xbel reads the shared recent files list, removes entries and clears it. Edits are written through a temporary file and checked before they replace the original.
* database stores favorites, tags and open history in SQLite. It uses a schema version number for upgrades, moves a corrupt file aside, refuses a newer schema and can export and import JSON.
* groups holds the built in and custom groups. The JSON layout follows Thingy's groups file, with a few extra built in groups.
* search turns the search box text into a query and matches documents. sorting orders them.
* scanner is a pure function that merges the recent list, KDE's list, favorites, tags and history into one list of documents. It applies exclusions and limits, and never looks at files on network mounts.
* index wraps the scanner for the window. It scans on a worker thread, joins quick repeated refreshes into one, and refreshes again when the recent list changes or every five minutes.
* thumbnails follows the freedesktop thumbnail cache rules and runs `.thumbnailer` helpers with an argument list and a time limit.
* launcher opens files through `xdg-open`, shows them in the file manager and moves them to the trash. It refuses to open programs.
* progress reads the reading progress that Xreader stores in file metadata through GIO. It is optional and is skipped when GIO is not available.
* settings and thingyimport handle the settings file and the Thingy import.

## Command line (src/cli)

Each command is a small function on top of the core library. The command line works without a display.

## Window (src/gui)

* documentmodel holds the documents and a proxy applies the group, tag, search and sort.
* carddelegate paints grid cards and list rows. The list view is virtualised, so large libraries stay responsive.
* thumbnailloader loads thumbnails on a thread pool, newest request first, and draws a coloured page placeholder until the picture is ready.
* sidebar, detailspanel, quickopen and the dialogs are small, separate widgets.
* mainwindow ties them together. singleinstance lets a second start hand over to the running window.

## Choices worth knowing

* Qt 6 only, with SQLite through QtSql and background work through QtConcurrent.
* The window never touches the disk on the main thread for scanning or thumbnails.
* The recent list is the source of truth for what was opened. Sheaf keeps only extras (favorites, tags and counts) in its own database, so removing Sheaf leaves your system as it was.
