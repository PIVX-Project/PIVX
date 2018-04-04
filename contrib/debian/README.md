
Debian
====================
This directory contains files used to package vsyncd/vsync-qt
for Debian-based Linux systems. If you compile vsyncd/vsync-qt yourself, there are some useful files here.

## vsync: URI support ##


vsync-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install vsync-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your vsyncqt binary to `/usr/bin`
and the `../../share/pixmaps/vsync128.png` to `/usr/share/pixmaps`

vsync-qt.protocol (KDE)

