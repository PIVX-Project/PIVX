
Debian
====================
This directory contains files used to package Sombed/Sombe-qt
for Debian-based Linux systems. If you compile Sombed/Sombe-qt yourself, there are some useful files here.

## Sombe: URI support ##


Sombe-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install Sombe-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your Sombe-qt binary to `/usr/bin`
and the `../../share/pixmaps/Sombe128.png` to `/usr/share/pixmaps`

Sombe-qt.protocol (KDE)

