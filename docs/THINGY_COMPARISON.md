# How Sheaf relates to Thingy

Thingy is the Linux Mint application for quick access to recent and favorite documents. Its project page describes it as an XApp that is meant to work on Cinnamon, MATE, Xfce, GNOME and other desktops, with code under the GPLv3 and translations handled through Launchpad.

## What this document covers

GitHub blocked me from reading Thingy's source files when I wrote this, so this page makes no claims about bugs or internals of Thingy. It only describes what Sheaf keeps from Thingy, what Sheaf does differently by design, and what is compatible. If you review Thingy's code and find specific faults, add them here with links.

## What Sheaf keeps

* The core idea of one place for recent and favorite documents.
* Group names and the groups file layout, so your hidden groups, custom groups and order can be imported.
* Favorites, which Thingy keeps in the `org.x.apps.favorites` setting. `sheaf import-thingy` reads them through `gsettings`.

## What Sheaf does differently by design

* Written from scratch in C++ with Qt 6, under the MIT License. No Thingy code is included. Sheaf only reads Thingy's data.
* Favorites, tags and history live in a database that is checked on start, with damaged files moved aside.
* Files on network mounts are not touched while building the list, so an unreachable server cannot freeze the window.
* Work that can be slow, such as scanning and making thumbnails, happens on background threads.
* Missing files stay visible and can be relinked.
* Search covers the whole library and has operators. Tags and extra groups are new.
* Programs are never started as documents.
* A command line tool, export and import, and a quick open window.

## Compatibility notes

* Thingy's group called documents is the PDF group in Sheaf, so imported settings keep the same meaning.
* Sheaf does not change Thingy's data. Both can be installed together.
