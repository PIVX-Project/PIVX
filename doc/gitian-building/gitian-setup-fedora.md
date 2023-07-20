Setting Up Gitian on Fedora and CentOS
============================

Fedora and CentOS support is still experimental. Testing has been done on Fedora 29+ and CentOS 7

The following steps will be performed using the terminal

<!-- markdown-toc start -->
**Table of Contents**

- [Required system packages](#required-system-packages)
    - [Python 3 on CentOS](#python-3-on-centos)
    - [Outdated Git on CentOS](#outdated-git-on-centos)
    - [Configuring Git](#configuring-git)
- [Fetching the Build Script](#fetching-the-build-script)
- [Initial Gitian Setup](#initial-gitian-setup)

<!-- markdown-toc end -->

Required System packages
-------------------------

The only base package that is absolutely required prior to using the gitian-build.py script is Python 3. All other packages will be installed automatically. That said, there are a few caveats as noted below:

#### Python 3 on CentOS

CentOS still ships with Python 2.7, which is incompatible with the gitian-build.py script. Python 3 can be installed with the following steps:

1. Install the SCL release file

        sudo yum install centos-release-scl

2. Install Python 3.6 (latest available as of Aug 2019)

        sudo yum install rh-python36

3. Make Python 3 available at login (optional)

        echo "source /opt/rh/rh-python36/enable" >> ~/.bashrc

4. Make Python 3 available for the current terminal session

        scl enable rh-python36 bash

#### Outdated Git on CentOS

Like as with Python, CentOS ships with an ancient version of Git (1.8.x), which is also not compatible with the gitian-build.py script. The script will detect the old version, and automatically update the system to a newer version of Git.

If you are currently using the default system Git in other work flows, you may need to learn about whats changed in recent versions of Git.

#### Configuring Git

Some basic configuration of Git to set your name and email is necessary. In the below command examples, it is good practice to use your GitHub username and email address:

```bash
git config --global user.name "GITHUB_USERNAME"
git config --global user.email "MY_NAME@example.com"
```

Fetching the Build Script
--------------------------

The build script we'll be using is contained in the hemis github repository ([contrib/gitian-build.py](https://github.com/hemis-project/hemis/blob/master/contrib/gitian-build.py)). Since this is a completely fresh environment, we haven't yet cloned the hemis repository and will need to fetch this script with the following commands:

```bash
curl -L -O https://raw.githubusercontent.com/hemis-Project/hemis/master/contrib/gitian-build.py
chmod +x gitian-build.py
```

This will place the script in your home directory and make it executable.

*Note: Changes to the script in the repository won't be automatically applied after fetching with the above commands. It is good practice to periodically re-run the above commands to ensure your version of the script is always up to date.*

Initial Gitian Setup
-------------------------

Now that the script has been downloaded to your home directory, its time to run it in setup mode. This will perform the following actions:

- Install the necessary system packages for gitian (namely Docker and it's related cli tools).
- Clone the gitian-builder, gitian.sigs, hemis-detached-sigs, and hemis GitHub repos.
- Configure proper user/group permissions for running gitian with Docker
- Create a base Docker image.

Run the following command:

```bash
./gitian-build.py --docker --setup
```
*The `--docker` option instructs the script to use Docker, and the `--setup` option instructs the script to run the setup procedure.*

If Docker is not already installed on the system, it will be installed, configured to start automatically at system startup, and your user will be added to the `docker` group. Afterwards, you will be prompted to restart your system. This is necessary in order for the user group change to take effect.

After restarting your computer, run the command again to finish the setup process

```bash
./gitian-build.py --docker --setup
```