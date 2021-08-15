#!/bin/bash
set -euxo pipefail
PATH_SAVE=$(echo  "$PATH") # save original Path
PATH=$(echo "$PATH" | sed -e 's/:\/mnt.*//g') # strip out problematic Windows %PATH% imported var
sudo bash -c "echo 0 > /proc/sys/fs/binfmt_misc/status" # Disable WSL support for Win32 applications.
git clean -dxf
cd depends
make HOST=x86_64-w64-mingw32
cd ..
./autogen.sh # not required when building from tarball
CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site ./configure --prefix=/ --disable-online-rust --with-qrencode --with-zmq --with-gui
make
sudo bash -c "echo 1 > /proc/sys/fs/binfmt_misc/status" # Enable WSL support for Win32 applications.
PATH=$(echo  "$PATH_SAVE") # restore original Path