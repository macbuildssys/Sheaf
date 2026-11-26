<p align="center"><img src="docs/logo.png" alt="Sheaf logo" width="160"></p>

# Sheaf

Sheaf is a library for the files you recently opened and the ones you marked as favorites. That includes documents, PDFs, books, pictures, audio, video and anything else your apps have opened. It gives you thumbnails, tags, groups and fast search, and it works on any desktop that follows the freedesktop standards.


## What it does

* Reads the standard recent files list that your other applications already write, so there is nothing to set up. It can also read KDE's recent files folder on a best effort basis.
* Shows files as a grid of thumbnails or as a list, with a zoom slider, an optional details panel and sorting by last used, name, date modified, size or type.
* Favorites, tags and your own groups. Groups can match by file type, by folder or by tag.
* One search box with a small search language: plain words, "quoted phrases", a minus sign to exclude, and tag:, type:, in:, is:fav and is:missing. Search ignores case and accents and looks through the whole library.
* Quick open window (Ctrl+K, or run `sheaf --quick` from a keyboard shortcut) for jumping to a file without the main window.
* Everything is in the right click menu. Right click a file for what you can do with it, or right click the empty space, the sidebar or the toolbar for settings, groups, view options and more. There are no extra buttons in the way. Hover over a file and click its star to make it a favorite.
* Command line tool for scripting: list, open, favorite, tag, forget, clean, export and import.
* Imports favorites and groups from Linux Mint Thingy.
* Shows reading progress for PDF and EPUB files when Xreader (the document viewer from Linux Mint and the XApps) has stored it.
* Only files that still exist are listed. Entries for files that were moved or deleted are left out.

## Install

Download the `.deb` and install it:

    sudo apt install ./sheaf_amd64.deb 
    or
    sudo apt install sheaf_amd64.deb

Sheaf needs Qt 6.2 or newer. That means Debian 13, Ubuntu 24.04, Kubuntu 24.04, Kali and newer releases. Older releases that only ship Qt 5 are not supported.

## License

Distributed under the MIT License. See [LICENSE](LICENSE).
