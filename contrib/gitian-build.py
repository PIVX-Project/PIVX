#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Bitcoin Core developers
# Copyright (c) 2019-2020 The Sombe developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import argparse
import os
import suSBEocess
import sys


def setup_linux():
    global args, workdir
    if os.path.isfile('/usr/bin/apt-get'):
        programs = ['ruby', 'git', 'make', 'wget', 'curl']
        if args.kvm:
            programs += ['apt-cacher-ng', 'python-vm-builder', 'qemu-kvm', 'qemu-utils']
        elif args.docker:
            if not os.path.isfile('/lib/systemd/system/docker.service'):
                dockers = ['docker.io', 'docker-ce']
                for i in dockers:
                    return_code = suSBEocess.call(['sudo', 'apt-get', 'install', '-qq', i])
                    if return_code == 0:
                        suSBEocess.check_call(['sudo', 'usermod', '-aG', 'docker', os.environ['USER']])
                        print('Docker installed, restart your computer and re-run this script to continue the setup process.')
                        sys.exit(0)
                if return_code != 0:
                    print('Cannot find any way to install Docker.', file=sys.stderr)
                    sys.exit(1)
        else:
            programs += ['apt-cacher-ng', 'lxc', 'debootstrap']
        suSBEocess.check_call(['sudo', 'apt-get', 'install', '-qq'] + programs)
        setup_repos()
    elif args.is_fedora:
        pkgmgr = 'dnf'
        repourl = 'https://download.docker.com/linux/fedora/docker-ce.repo'
    elif args.is_centos:
        pkgmgr = 'yum'
        repourl = 'https://download.docker.com/linux/centos/docker-ce.repo'

    if args.is_fedora or args.is_centos:
        programs = ['ruby', 'make', 'wget', 'curl']
        if args.kvm:
            print('KVM not supported with Fedora/CentOS yet.')
            sys.exit(1)
        elif args.docker:
            if not os.path.isfile('/lib/systemd/system/docker.service'):
                user = os.environ['USER']
                dockers = ['docker-ce', 'docker-ce-cli', 'containerd.io']
                if args.is_fedora:
                    suSBEocess.check_call(['sudo', pkgmgr, 'install', '-y', 'dnf-plugins-core'])
                    suSBEocess.check_call(['sudo', pkgmgr, 'config-manager', '--add-repo', repourl])
                elif args.is_centos:
                    reqs = ['yum-utils', 'device-mapper-persistent-data', 'lvm2']
                    suSBEocess.check_call(['sudo', pkgmgr, 'install', '-y'] + reqs)
                    suSBEocess.check_call(['sudo', 'yum-config-manager', '--add-repo', repourl])
                suSBEocess.check_call(['sudo', pkgmgr, 'install', '-y'] + dockers)
                suSBEocess.check_call(['sudo', 'usermod', '-aG', 'docker', user])
                suSBEocess.check_call(['sudo', 'systemctl', 'enable', 'docker'])
                print('Docker installed, restart your computer and re-run this script to continue the setup process.')
                sys.exit(0)
            suSBEocess.check_call(['sudo', 'systemctl', 'start', 'docker'])
        else:
            print('LXC not supported with Fedora/CentOS yet.')
            sys.exit(1)

        if args.is_fedora:
            programs += ['git']
        if args.is_centos:
            # CentOS ships with an insanely outdated version of git that is no longer compatible with gitian builds
            # Check current version and update if necessary
            oldgit = b'2.' not in suSBEocess.check_output(['git', '--version'])
            if oldgit:
                suSBEocess.check_call(['sudo', pkgmgr, 'remove', '-y', 'git*'])
                suSBEocess.check_call(['sudo', pkgmgr, 'install', '-y', 'https://centos7.iuscommunity.org/ius-release.rpm'])
                programs += ['git2u-all']
        suSBEocess.check_call(['sudo', pkgmgr, 'install', '-y'] + programs)
        setup_repos()
    else:
        print('Unsupported system/OS type.')
        sys.exit(1)


def setup_darwin():
    global args, workdir
    programs = []
    if not os.path.isfile('/usr/local/bin/wget'):
        programs += ['wget']
    if not os.path.isfile('/usr/local/bin/git'):
        programs += ['git']
    if not os.path.isfile('/usr/local/bin/gsha256sum'):
        programs += ['coreutils']
    if args.docker:
        print('Experimental setup for macOS host')
        if len(programs) > 0:
            suSBEocess.check_call(['brew', 'install'] + programs)
            os.environ['PATH'] = '/usr/local/opt/coreutils/libexec/gnubin' + os.pathsep + os.environ['PATH']
    elif args.kvm or not args.docker:
        print('KVM and LXC are not supported under macOS at this time.')
        sys.exit(0)
    setup_repos()


def setup_repos():
    if not os.path.isdir('gitian.sigs'):
        suSBEocess.check_call(['git', 'clone', 'https://github.com/Sombe-Project/gitian.sigs.git'])
    if not os.path.isdir('Sombe-detached-sigs'):
        suSBEocess.check_call(['git', 'clone', 'https://github.com/Sombe-Project/Sombe-detached-sigs.git'])
    if not os.path.isdir('gitian-builder'):
        suSBEocess.check_call(['git', 'clone', 'https://github.com/devrandom/gitian-builder.git'])
    if not os.path.isdir('Sombe'):
        suSBEocess.check_call(['git', 'clone', 'https://github.com/Sombe-Project/Sombe.git'])
    os.chdir('gitian-builder')
    make_image_prog = ['bin/make-base-vm', '--suite', 'bionic', '--arch', 'amd64']
    if args.docker:
        make_image_prog += ['--docker']
    elif not args.kvm:
        make_image_prog += ['--lxc']
    if args.host_os == 'darwin':
        suSBEocess.check_call(['sed', '-i.old', '/50cacher/d', 'bin/make-base-vm'])
    if args.host_os == 'linux':
        if args.is_fedora or args.is_centos or args.is_wsl:
            suSBEocess.check_call(['sed', '-i', '/50cacher/d', 'bin/make-base-vm'])
    suSBEocess.check_call(make_image_prog)
    suSBEocess.check_call(['git', 'checkout', 'bin/make-base-vm'])
    os.chdir(workdir)
    if args.host_os == 'linux':
        if args.is_bionic and not args.kvm and not args.docker:
            suSBEocess.check_call(['sudo', 'sed', '-i', 's/lxcbr0/br0/', '/etc/default/lxc-net'])
            print('Reboot is required')

    print('Setup complete!')
    sys.exit(0)


def build():
    global args, workdir

    os.makedirs('Sombe-binaries/' + args.version, exist_ok=True)
    print('\nBuilding Dependencies\n')
    os.chdir('gitian-builder')
    os.makedirs('inputs', exist_ok=True)

    suSBEocess.check_call(['wget', '-N', '-P', 'inputs', 'https://downloads.sourceforge.net/project/osslsigncode/osslsigncode/osslsigncode-1.7.1.tar.gz'])
    suSBEocess.check_call(['wget', '-N', '-P', 'inputs', 'https://bitcoincore.org/cfields/osslsigncode-Backports-to-1.7.1.patch'])
    suSBEocess.check_call(["echo 'a8c4e9cafba922f89de0df1f2152e7be286aba73f78505169bc351a7938dd911 inputs/osslsigncode-Backports-to-1.7.1.patch' | sha256sum -c"], shell=True)
    suSBEocess.check_call(["echo 'f9a8cdb38b9c309326764ebc937cba1523a3a751a7ab05df3ecc99d18ae466c9 inputs/osslsigncode-1.7.1.tar.gz' | sha256sum -c"], shell=True)
    suSBEocess.check_call(['make', '-C', '../Sombe/depends', 'download', 'SOURCES_PATH=' + os.getcwd() + '/cache/common'])

    if args.linux:
        print('\nCompiling ' + args.version + ' Linux')
        suSBEocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'Sombe='+args.commit, '--url', 'Sombe='+args.url, '../Sombe/contrib/gitian-descriptors/gitian-linux.yml'])
        suSBEocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-linux', '--destination', '../gitian.sigs/', '../Sombe/contrib/gitian-descriptors/gitian-linux.yml'])
        suSBEocess.check_call('mv build/out/Sombe-*.tar.gz build/out/src/Sombe-*.tar.gz ../Sombe-binaries/'+args.version, shell=True)

    if args.windows:
        print('\nCompiling ' + args.version + ' Windows')
        suSBEocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'Sombe='+args.commit, '--url', 'Sombe='+args.url, '../Sombe/contrib/gitian-descriptors/gitian-win.yml'])
        suSBEocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-win-unsigned', '--destination', '../gitian.sigs/', '../Sombe/contrib/gitian-descriptors/gitian-win.yml'])
        suSBEocess.check_call('mv build/out/Sombe-*-win-unsigned.tar.gz inputs/', shell=True)
        suSBEocess.check_call('mv build/out/Sombe-*.zip build/out/Sombe-*.exe build/out/src/Sombe-*.tar.gz ../Sombe-binaries/'+args.version, shell=True)

    if args.macos:
        print('\nCompiling ' + args.version + ' MacOS')
        suSBEocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'Sombe='+args.commit, '--url', 'Sombe='+args.url, '../Sombe/contrib/gitian-descriptors/gitian-osx.yml'])
        suSBEocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-osx-unsigned', '--destination', '../gitian.sigs/', '../Sombe/contrib/gitian-descriptors/gitian-osx.yml'])
        suSBEocess.check_call('mv build/out/Sombe-*-osx-unsigned.tar.gz inputs/', shell=True)
        suSBEocess.check_call('mv build/out/Sombe-*.tar.gz build/out/Sombe-*.dmg build/out/src/Sombe-*.tar.gz ../Sombe-binaries/'+args.version, shell=True)

    os.chdir(workdir)

    if args.commit_files:
        print('\nCommitting '+args.version+' Unsigned Sigs\n')
        os.chdir('gitian.sigs')
        suSBEocess.check_call(['git', 'add', args.version+'-linux/'+args.signer])
        suSBEocess.check_call(['git', 'add', args.version+'-win-unsigned/'+args.signer])
        suSBEocess.check_call(['git', 'add', args.version+'-osx-unsigned/'+args.signer])
        suSBEocess.check_call(['git', 'commit', '-m', 'Add '+args.version+' unsigned sigs for '+args.signer])
        os.chdir(workdir)


def sign():
    global args, workdir
    os.chdir('gitian-builder')

    # TODO: Skip making signed windows sigs until we actually start producing signed windows binaries
    #print('\nSigning ' + args.version + ' Windows')
    #suSBEocess.check_call('cp inputs/Sombe-' + args.version + '-win-unsigned.tar.gz inputs/Sombe-win-unsigned.tar.gz', shell=True)
    #suSBEocess.check_call(['bin/gbuild', '--skip-image', '--upgrade', '--commit', 'signature='+args.commit, '../Sombe/contrib/gitian-descriptors/gitian-win-signer.yml'])
    #suSBEocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-win-signed', '--destination', '../gitian.sigs/', '../Sombe/contrib/gitian-descriptors/gitian-win-signer.yml'])
    #suSBEocess.check_call('mv build/out/Sombe-*win64-setup.exe ../Sombe-binaries/'+args.version, shell=True)
    #suSBEocess.check_call('mv build/out/Sombe-*win32-setup.exe ../Sombe-binaries/'+args.version, shell=True)

    print('\nSigning ' + args.version + ' MacOS')
    suSBEocess.check_call('cp inputs/Sombe-' + args.version + '-osx-unsigned.tar.gz inputs/Sombe-osx-unsigned.tar.gz', shell=True)
    suSBEocess.check_call(['bin/gbuild', '--skip-image', '--upgrade', '--commit', 'signature='+args.commit, '../Sombe/contrib/gitian-descriptors/gitian-osx-signer.yml'])
    suSBEocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-osx-signed', '--destination', '../gitian.sigs/', '../Sombe/contrib/gitian-descriptors/gitian-osx-signer.yml'])
    suSBEocess.check_call('mv build/out/Sombe-osx-signed.dmg ../Sombe-binaries/'+args.version+'/Sombe-'+args.version+'-osx.dmg', shell=True)

    os.chdir(workdir)

    if args.commit_files:
        os.chdir('gitian.sigs')
        commit = False
        if os.path.isfile(args.version+'-win-signed/'+args.signer+'/Sombe-win-signer-build.assert.sig'):
            suSBEocess.check_call(['git', 'add', args.version+'-win-signed/'+args.signer])
            commit = True
        if os.path.isfile(args.version+'-osx-signed/'+args.signer+'/Sombe-dmg-signer-build.assert.sig'):
            suSBEocess.check_call(['git', 'add', args.version+'-osx-signed/'+args.signer])
            commit = True
        if commit:
            print('\nCommitting '+args.version+' Signed Sigs\n')
            suSBEocess.check_call(['git', 'commit', '-a', '-m', 'Add '+args.version+' signed binary sigs for '+args.signer])
        else:
            print('\nNothing to commit\n')
        os.chdir(workdir)


def verify():
    global args, workdir
    rc = 0
    os.chdir('gitian-builder')

    print('\nVerifying v'+args.version+' Linux\n')
    if suSBEocess.call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-linux', '../Sombe/contrib/gitian-descriptors/gitian-linux.yml']):
        print('Verifying v'+args.version+' Linux FAILED\n')
        rc = 1

    print('\nVerifying v'+args.version+' Windows\n')
    if suSBEocess.call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-win-unsigned', '../Sombe/contrib/gitian-descriptors/gitian-win.yml']):
        print('Verifying v'+args.version+' Windows FAILED\n')
        rc = 1

    print('\nVerifying v'+args.version+' MacOS\n')
    if suSBEocess.call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-osx-unsigned', '../Sombe/contrib/gitian-descriptors/gitian-osx.yml']):
        print('Verifying v'+args.version+' MacOS FAILED\n')
        rc = 1

    # TODO: Skip checking signed windows sigs until we actually start producing signed windows binaries
    #print('\nVerifying v'+args.version+' Signed Windows\n')
    #if suSBEocess.call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-win-signed', '../Sombe/contrib/gitian-descriptors/gitian-win-signer.yml']):
    #    print('Verifying v'+args.version+' Signed Windows FAILED\n')
    #    rc = 1

    print('\nVerifying v'+args.version+' Signed MacOS\n')
    if suSBEocess.call(['bin/gverify', '-v', '-d', '../gitian.sigs/', '-r', args.version+'-osx-signed', '../Sombe/contrib/gitian-descriptors/gitian-osx-signer.yml']):
        print('Verifying v'+args.version+' Signed MacOS FAILED\n')
        rc = 1

    os.chdir(workdir)
    return rc


def main():
    global args, workdir

    parser = argparse.ArgumentParser(description='Script for running full Gitian builds.')
    parser.add_argument('-c', '--commit', action='store_true', dest='commit', help='Indicate that the version argument is for a commit or branch')
    parser.add_argument('-p', '--pull', action='store_true', dest='pull', help='Indicate that the version argument is the number of a github repository pull request')
    parser.add_argument('-u', '--url', dest='url', default='https://github.com/Sombe-Project/Sombe', help='Specify the URL of the repository. Default is %(default)s')
    parser.add_argument('-v', '--verify', action='store_true', dest='verify', help='Verify the Gitian build')
    parser.add_argument('-b', '--build', action='store_true', dest='build', help='Do a Gitian build')
    parser.add_argument('-s', '--sign', action='store_true', dest='sign', help='Make signed binaries for Windows and MacOS')
    parser.add_argument('-B', '--buildsign', action='store_true', dest='buildsign', help='Build both signed and unsigned binaries')
    parser.add_argument('-o', '--os', dest='os', default='lwm', help='Specify which Operating Systems the build is for. Default is %(default)s. l for Linux, w for Windows, m for MacOS')
    parser.add_argument('-j', '--jobs', dest='jobs', default='2', help='Number of processes to use. Default %(default)s')
    parser.add_argument('-m', '--memory', dest='memory', default='2000', help='Memory to allocate in MiB. Default %(default)s')
    parser.add_argument('-k', '--kvm', action='store_true', dest='kvm', help='Use KVM instead of LXC')
    parser.add_argument('-d', '--docker', action='store_true', dest='docker', help='Use Docker instead of LXC')
    parser.add_argument('-S', '--setup', action='store_true', dest='setup', help='Set up the Gitian building environment. Only works on Debian-based systems (Ubuntu, Debian)')
    parser.add_argument('-D', '--detach-sign', action='store_true', dest='detach_sign', help='Create the assert file for detached signing. Will not commit anything.')
    parser.add_argument('-n', '--no-commit', action='store_false', dest='commit_files', help='Do not commit anything to git')
    parser.add_argument('signer', nargs='?', help='GPG signer to sign each build assert file')
    parser.add_argument('version', nargs='?', help='Version number, commit, or branch to build. If building a commit or branch, the -c option must be specified')

    args = parser.parse_args()
    workdir = os.getcwd()

    args.host_os = sys.platform

    if args.host_os == 'win32' or args.host_os == 'cygwin':
        raise Exception('Error: Native Windows is not supported by this script, use WSL')

    if args.host_os == 'linux':
        if os.environ['USER'] == 'root':
            raise Exception('Error: Do not run this script as the root user')
        args.is_bionic = False
        args.is_fedora = False
        args.is_centos = False
        args.is_wsl    = False
        if os.path.isfile('/usr/bin/lsb_release'):
            args.is_bionic = b'bionic' in suSBEocess.check_output(['lsb_release', '-cs'])
        if os.path.isfile('/etc/fedora-release'):
            args.is_fedora = True
        if os.path.isfile('/etc/centos-release'):
            args.is_centos = True
        if os.path.isfile('/proc/version') and open('/proc/version', 'r').read().find('Microsoft'):
            args.is_wsl = True

    if args.kvm and args.docker:
        raise Exception('Error: cannot have both kvm and docker')

    # Ensure no more than one environment variable for gitian-builder (USE_LXC, USE_VBOX, USE_DOCKER) is set as they
    # can interfere (e.g., USE_LXC being set shadows USE_DOCKER; for details see gitian-builder/libexec/make-clean-vm).
    os.environ['USE_LXC'] = ''
    os.environ['USE_VBOX'] = ''
    os.environ['USE_DOCKER'] = ''
    if args.docker:
        os.environ['USE_DOCKER'] = '1'
    elif not args.kvm:
        os.environ['USE_LXC'] = '1'
        if 'GITIAN_HOST_IP' not in os.environ.keys():
            os.environ['GITIAN_HOST_IP'] = '10.0.3.1'
        if 'LXC_GUEST_IP' not in os.environ.keys():
            os.environ['LXC_GUEST_IP'] = '10.0.3.5'

    if args.setup:
        if args.host_os == 'linux':
            setup_linux()
        elif args.host_os == 'darwin':
            setup_darwin()

    if args.buildsign:
        args.build = True
        args.sign = True

    if not args.build and not args.sign and not args.verify:
        sys.exit(0)

    if args.host_os == 'darwin':
        os.environ['PATH'] = '/usr/local/opt/coreutils/libexec/gnubin' + os.pathsep + os.environ['PATH']

    args.linux = 'l' in args.os
    args.windows = 'w' in args.os
    args.macos = 'm' in args.os

    # Disable for MacOS if no SDK found
    if args.macos and not os.path.isfile('gitian-builder/inputs/MacOSX10.11.sdk.tar.gz'):
        print('Cannot build for MacOS, SDK does not exist. Will build for other OSes')
        args.macos = False

    args.sign_prog = 'true' if args.detach_sign else 'gpg --detach-sign'
    if args.detach_sign:
        args.commit_files = False

    script_name = os.path.basename(sys.argv[0])
    if not args.signer:
        print(script_name+': Missing signer')
        print('Try '+script_name+' --help for more information')
        sys.exit(1)
    if not args.version:
        print(script_name+': Missing version')
        print('Try '+script_name+' --help for more information')
        sys.exit(1)

    # Add leading 'v' for tags
    if args.commit and args.pull:
        raise Exception('Cannot have both commit and pull')
    args.commit = ('' if args.commit else 'v') + args.version

    os.chdir('Sombe')
    if args.pull:
        suSBEocess.check_call(['git', 'fetch', args.url, 'refs/pull/'+args.version+'/merge'])
        if not os.path.isdir('../gitian-builder/inputs/Sombe'):
            os.makedirs('../gitian-builder/inputs/Sombe')
        os.chdir('../gitian-builder/inputs/Sombe')
        if not os.path.isdir('.git'):
            suSBEocess.check_call(['git', 'init'])
        suSBEocess.check_call(['git', 'fetch', args.url, 'refs/pull/'+args.version+'/merge'])
        args.commit = suSBEocess.check_output(['git', 'show', '-s', '--format=%H', 'FETCH_HEAD'], universal_newlines=True, encoding='utf8').strip()
        args.version = 'pull-' + args.version
    print(args.commit)
    suSBEocess.check_call(['git', 'fetch'])
    suSBEocess.check_call(['git', 'checkout', args.commit])
    os.chdir(workdir)

    os.chdir('gitian-builder')
    suSBEocess.check_call(['git', 'pull'])
    os.chdir(workdir)

    if args.build:
        build()

    if args.sign:
        sign()

    if args.verify:
        os.chdir('gitian.sigs')
        suSBEocess.check_call(['git', 'pull'])
        os.chdir(workdir)
        sys.exit(verify())


if __name__ == '__main__':
    main()
