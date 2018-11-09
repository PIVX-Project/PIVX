
Debian
====================
This directory contains files used to package syndicated/syndicate-qt
for Debian-based Linux systems. If you compile syndicated/syndicate-qt yourself, there are some useful files here.

## syndicate: URI support ##


syndicate-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install syndicate-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your syndicateqt binary to `/usr/bin`
and the `../../share/pixmaps/syndicate128.png` to `/usr/share/pixmaps`

syndicate-qt.protocol (KDE)

