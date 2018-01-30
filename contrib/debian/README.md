
Debian
====================
This directory contains files used to package colxd/colx-qt
for Debian-based Linux systems. If you compile colxd/colx-qt yourself, there are some useful files here.

## colx: URI support ##


colx-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install colx-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your colxqt binary to `/usr/bin`
and the `../../share/pixmaps/colx128.png` to `/usr/share/pixmaps`

colx-qt.protocol (KDE)

