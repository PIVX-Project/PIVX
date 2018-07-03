
Debian
====================
This directory contains files used to package darknetd/darknet-qt
for Debian-based Linux systems. If you compile darknetd/darknet-qt yourself, there are some useful files here.

## darknet: URI support ##


darknet-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install darknet-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your darknetqt binary to `/usr/bin`
and the `../../share/pixmaps/darknet128.png` to `/usr/share/pixmaps`

darknet-qt.protocol (KDE)

