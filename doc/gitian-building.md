Gitian building
================

*Setup instructions for a gitian build of hemis Core using a VM or physical system.*

Gitian is the deterministic build process that is used to build the hemis
Core executables. It provides a way to be reasonably sure that the
executables are really built from source on GitHub. It also makes sure that
the same, tested dependencies are used and statically built into the executable.

Multiple developers build the source code by following a specific descriptor
("recipe"), cryptographically sign the result, and upload the resulting signature.
These results are compared and only if they match, the build is accepted and uploaded
to the hemis GitHub release page.

More independent gitian builders are needed, which is why this guide exists.
It is preferred to follow these steps yourself instead of using someone else's
VM image to avoid 'contaminating' the build.

Table of Contents
------------------

- [Preparing the Gitian builder host](#preparing-the-gitian-builder-host)
  - [macOS Builds](#macos-builds)
- [Initial Gitian Setup](#initial-gitian-setup)
- [Building hemis Core](#building-hemis-core)
- [Signing externally](#signing-externally)
- [Uploading signatures](#uploading-signatures)

Preparing the Gitian builder host
---------------------------------

The first step is to prepare the host environment that will be used to perform the Gitian builds.
This guide explains how to set up the environment, and how to start the builds.

Gitian builds are known to be working on recent versions of Windows 10, Debian, Ubuntu, Fedora and macOS.
If your machine is already running one of those operating systems, you can perform Gitian builds on the actual hardware.
Alternatively, you can install one of the supported operating systems in a virtual machine.

Any kind of virtualization can be used, for example:
- [Docker](https://www.docker.com/) (covered by this guide)
- [KVM](http://www.linux-kvm.org/page/Main_Page)
- [LXC](https://linuxcontainers.org/)

Please refer to the following documents to set up the operating systems and Gitian.

|                          | Ubuntu/Debian                                                      | Fedora/CentOS                                                      | Windows                                                               | macOS
|--------------------------|--------------------------------------------------------------------|--------------------------------------------------------------------|-----------------------------------------------------------------------|----------------------------------------------------------------
| Setup Docker             |                                                                    |                                                                    | [Setup Docker for Windows](./gitian-building/docker-setup-windows.md) | [Setup Docker for macOS](./gitian-building/docker-setup-mac.md)
| Setup WSL (windows only) |                                                                    |                                                                    | [Setup WSL for Windows](./gitian-building/wsl-setup-windows.md)       |
| Setup Gitian             | [Setup Gitian on Ubuntu](./gitian-building/gitian-setup-ubuntu.md) | [Setup Gitian on Fedora](./gitian-building/gitian-setup-fedora.md) | [Setup Gitian on Windows](./gitian-building/gitian-setup-windows.md)  | [Setup Gitian on macOS](./gitian-building/gitian-setup-mac.md)

Note for users wishing to use LXC: A version of `lxc-execute` higher or equal to 2.1.1 is required.
You can check the version with `lxc-execute --version`.
On Debian you might have to compile a suitable version of lxc or you can use Ubuntu 18.04 or higher instead of Debian as the host.

#### macOS Builds

In order to build and sign for macOS, you need to download the free SDK and extract a file. The steps are described [here](../contrib/macdeploy/README.md#sdk-extraction). Alternatively, you can skip the macOS build by adding `--os=lw` below.

Initial Gitian Setup
--------------------

The `gitian-build.py` script is designed to checkout different release tags, commits, branches, or pull requests. The linked guides above cover the process of obtaining the script and doing the basic initial setup.

Building hemis Core
--------------------

The script allows you to build tags, commits, branches, and even pull requests. Below are some examples:

* To build the 3.3.0 release tag:
    ```bash
    export NAME=satoshi
    export VERSION=3.3.0
    ./gitian-build.py --docker -b $NAME $VERSION
    ```
* To Build the master branch:
    ```bash
    export NAME=satoshi
    export BRANCH=master
    ./gitian-build.py --docker -b -c --detach-sign $NAME $BRANCH
    ```
* To Build a specific commit:
    ```bash
    export NAME=satoshi
    export COMMIT=2269f10fd91b9dbfba0ecc43e4558b7590e3805c
    ./gitian-build.py --docker -b -c --detach-sign $NAME $COMMIT
    ```
* To Build a pull request:
    ```bash
    export NAME=satoshi
    export PRNUM=949
    ./gitian-build.py --docker -b -p --detach-sign $NAME $PRNUM
    ```
To speed up the build, add `-j 6 -m 6000` to the list of arguments, where `6` is the number of CPU cores you wish to use (cannot exceed what is available), and 6000 is a little bit less than then the MB's of RAM you have.

Note the use of the `--detach-sign` option in the examples above, which allows you to sign the build results on a machine other than the gitian host. Only release tags have their signatures signed and committed to a public repository.

The build process results in a number of `.assert` files in your local gitian.sigs repository. Additionally, if you omit the `--detach-sign` option, the script will attempt to sign and commit these files for you.

Signing Externally
--------------------

If your gitian host does not have your GPG private key installed, you will need to copy these uncommited changes to your host machine, where you can sign them:

```bash
gpg --output ${VERSION}-linux/${NAME}/hemis-linux-${VERSION%\.*}-build.assert.sig --detach-sign ${VERSION}-linux/$NAME/hemis-linux-${VERSION%\.*}-build.assert
gpg --output ${VERSION}-osx-unsigned/$NAME/hemis-osx-${VERSION%\.*}-build.assert.sig --detach-sign ${VERSION}-osx-unsigned/$NAME/hemis-osx-${VERSION%\.*}-build.assert
gpg --output ${VERSION}-win-unsigned/$NAME/hemis-win-${VERSION%\.*}-build.assert.sig --detach-sign ${VERSION}-win-unsigned/$NAME/hemis-win-${VERSION%\.*}-build.assert
```

Uploading Signatures
--------------------
Make a Pull Request (both the `.assert` and `.assert.sig` files) to the
[gitian.sigs](https://github.com/hemis-project/gitian.sigs/) repository:

```bash
git checkout -b ${VERSION}-not-codesigned
git commit -S -a -m "Add $NAME $VERSION non-code signed signatures"
git push --set-upstream $NAME $VERSION-not-codesigned
```

You can also mail the files to Fuzzbawls (fuzzbawls@hemis.org) and he will commit them.

```bash
gpg --detach-sign ${VERSION}-linux/${NAME}/hemis-linux-*-build.assert
gpg --detach-sign ${VERSION}-win-unsigned/${NAME}/hemis-win-*-build.assert
gpg --detach-sign ${VERSION}-osx-unsigned/${NAME}/hemis-osx-*-build.assert
```

You may have other .assert files as well (e.g. `signed` ones), in which case you should sign them too. You can see all of them by doing `ls ${VERSION}-*/${NAME}`.

This will create the `.sig` files that can be committed together with the `.assert` files to assert your
Gitian build.

 `./gitian-build.py --detach-sign -s $NAME $VERSION`

Make another pull request for these.