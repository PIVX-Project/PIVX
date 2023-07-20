Setting up Gitian on Windows WSL
=================================

Now that [Docker for Windows](./docker-setup-windows.md) and [WSL's Ubuntu 18.04 LTS](./wsl-setup-windows.md) have been installed and configured properly, you're ready to do the initial setup of the Gitian build system.

The following steps will be performed using the WSL terminal, which can be accessed by running the "Ubuntu 18.04 LTS" app from the Windows start menu.

<!-- markdown-toc start -->
**Table of Contents**

- [Required system packages](#required-system-packages)
    - [Configuring Git](#configuring-git)
- [Fetching the Build Script](#fetching-the-build-script)
- [Initial Gitian Setup](#initial-gitian-setup)

<!-- markdown-toc end -->

Required System Packages
-------------------------

Python3 and Git are the two base requirements that need to be met for our build setup.

Ubuntu 18.04 already includes python3 out of the box, so we only need to install `git` with the following command:

```bash
sudo apt install git
```

##### Configuring Git

Once Git is installed, you will need to do some basic configuration to set your name and email. In the below command examples, it is good practice to use your GitHub username and email address:

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

- Install the necessary system packages for gitian (namely the Docker cli tools).
- Clone the gitian-builder, gitian.sigs, hemis-detached-sigs, and hemis GitHub repos.
- Configure proper user/group permissions for running gitian with Docker
- Create a base Docker image.

Run the following command:

```bash
./gitian-build.py --docker --setup
```
*The `--docker` option instructs the script to use Docker, and the `--setup` option instructs the script to run the setup procedure.*

