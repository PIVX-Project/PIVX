
Debian
====================
This directory contains files used to package privixd/privix-qt
for Debian-based Linux systems. If you compile privixd/privix-qt yourself, there are some useful files here.

## vpx: URI support ##


privix-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install privix-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your vpxqt binary to `/usr/bin`
and the `../../share/pixmaps/vpx128.png` to `/usr/share/pixmaps`

privix-qt.protocol (KDE)

