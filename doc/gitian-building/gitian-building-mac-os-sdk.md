Gitian building macOS SDK
==========================

On the host machine, register for a free Apple [developer account](https://developer.apple.com/register/), then download the SDK [here](https://developer.apple.com/devcenter/download.action?path=/Developer_Tools/Xcode_7.3.1/Xcode_7.3.1.dmg).

macOS host
--------

Using macOS, you can mount the dmg, and then extract the SDK with:
```
  $ hdiutil attach Xcode_7.3.1.dmg
  $ tar -C /Volumes/Xcode/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/ -czf MacOSX10.11.sdk.tar.gz MacOSX10.11.sdk
```

Clean up the files you don't need:

```sh
diskutil unmount /Volumes/Xcode
rm Xcode_7.3.1.dmg
```

Non-macOS host:
--------

Alternatively, you can use 7zip and SleuthKit to extract the files one by one.
The script [extract-osx-sdk.sh](https://github.com/Sombe-project/Sombe/blob/master/contrib/macdeploy/extract-osx-sdk.sh) automates this. First ensure
the dmg file is in the current directory, and then run the script.

You may wish to delete the intermediate `5.hfs` file and `MacOSX10.11.sdk` (the directory) when
you've confirmed the extraction succeeded.

#### Ubuntu/Debian
```bash
sudo apt-get install p7zip-full sleuthkit
Sombe/contrib/macdeploy/extract-osx-sdk.sh
rm -rf 5.hfs MacOSX10.11.sdk
```

#### Fedora
```bash
sudo dnf install p7zip p7zip-plugins sleuthkit
Sombe/contrib/macdeploy/extract-osx-sdk.sh
rm -rf 5.hfs MacOSX10.11.sdk
```

#### CentOS
```bash
sudo yum install p7zip p7zip-plugins sleuthkit
Sombe/contrib/macdeploy/extract-osx-sdk.sh
rm -rf 5.hfs MacOSX10.11.sdk
```

Move SDK to Gitian's inputs directory:
----------------------
Move the resulting SDK archive to gitian-builder's inputs directory

```bash
mkdir -p gitian-builder/inputs
mv MacOSX10.11.sdk.tar.gz gitian-builder/inputs
```

Troubleshooting
---------------
See [README_osx.md](https://github.com/Sombe-project/Sombe/blob/master/doc/README_osx.md) for troubleshooting tips.
