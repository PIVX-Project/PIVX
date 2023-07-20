
Debian
====================
This directory contains files used to package hemisd/hemis-qt
for Debian-based Linux systems. If you compile hemisd/hemis-qt yourself, there are some useful files here.

## hemis: URI support ##


hemis-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install hemis-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your hemis-qt binary to `/usr/bin`
and the `../../share/pixmaps/hemis128.png` to `/usr/share/pixmaps`

hemis-qt.protocol (KDE)

