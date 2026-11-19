# User guide

## First start

Open Sheaf. It shows the files that your other applications recently opened, whatever their type. If you used Thingy, right click an empty part of the window, choose Settings, open the Data page and use the import button to bring over your favorites and groups.

## Everything is on right click

Sheaf has no row of buttons and no menu button to hunt through.

* Right click a file for what you can do with it: open, show in folder, favorite, tags, copy, remove from the list, move to trash and locate if it is missing.
* Right click the empty space, the sidebar or the toolbar for everything else: add files, quick open, sort, view options, groups, settings, refresh, shortcuts and about.
* Double click or press Enter to open a file.
* Hover over a file to see a star. Click it to make the file a favorite, and click it again to undo.

## Finding files

* Pick a group in the sidebar to see only that kind of file. The number beside each group is how many files it holds.
* Type in the search box, or press Ctrl+F or the slash key. Searching always looks through the whole library, not just the open group.
* Start typing while the list has focus and the text goes to the search box.
* Press Escape to clear the search.

Search words you can use:

* Plain words must all match, in any order. Accents and capital letters do not matter.
* "Quoted words" match as one phrase.
* A minus sign before a word leaves out files that contain it, for example `-draft`.
* `tag:work` finds files with that tag.
* `type:pdf` finds a file type by extension or kind.
* `in:invoices` finds files whose folder path contains the word.
* `is:fav` finds favorites and `is:missing` finds files that are gone.

## Opening and managing

* Enter opens the selected files. Opening more than ten at once asks first.
* Ctrl+D adds or removes a favorite. Ctrl+T edits tags.
* Ctrl+Shift+O shows the file in your file manager.
* Ctrl+C copies the file so you can paste it in a file manager. Ctrl+Shift+C copies the path.
* Delete takes a file off the recent list. The file stays where it is.
* Shift+Delete moves the file to the trash, after asking.
* Drop files onto the window to make them favorites.

## Files that moved or were deleted

Sheaf only lists files that still exist. Entries for files that were moved or deleted are left out, and Remove Missing Files on the Data page of Settings forgets them for good.

## Views

* Right click the empty space, then View, to switch between the grid and the list. The same buttons are in the toolbar. Ctrl+Shift+G is the grid and Ctrl+Shift+L is the list.
* Ctrl and the mouse wheel changes the thumbnail size.
* F9 shows or hides the details panel. The panel is for reading only, everything you can do is in the right click menu.
* Ctrl+1 to Ctrl+9 jump to the first nine groups.

## Quick open

Press Ctrl+K inside Sheaf, or add a keyboard shortcut in your desktop settings that runs `sheaf --quick`. Type a few letters, press Enter, and the file opens.

## Groups

Right click the sidebar and choose Groups. You can hide built in groups, change their order, and make your own that match file types, a folder or tags.

## Command line

    sheaf list "tag:work type:pdf"
    sheaf favorite add ~/Documents/plan.pdf
    sheaf tag add work ~/Documents/plan.pdf
    sheaf clean --dry-run
    sheaf export library.json

Run `man sheaf` for the full list.

## Settings

* Excluded folders are skipped when the list is built. Favorites are never excluded.
* Settings covers whether to include KDE's recent files, how many files to keep, thumbnail creation and the trash confirmation.
* The Data page can export and import your favorites and tags, import from Thingy, forget missing files and clear the recent list.
