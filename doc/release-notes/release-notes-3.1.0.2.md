hemis Core version *3.1.0.2* is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new mandatory bugfix release, specifically addressing a bug with windows OS gamemaster control wallets as well as a missed commit from our private repository affecting the budget amount. 

### Users upgrading to this version are encouraged to also read the detailed release notes for the previous [3.1.0](https://github.com/hemis-Project/hemis/releases/tag/v3.1.0) release as information will not be duplicated here.


Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>

Mandatory Update
==============

hemis Core v3.1.0.2 is a mandatory update for all users. This release contains new consensus rules and improvements that are not backwards compatible with older versions. Users will have a grace period of one week to update their clients before enforcement of this update is enabled.

Users updating from a previous version after Tuesday, May 8, 2018 12:00:00 AM GMT will require a full resync of their local blockchain from either the P2P network or by way of the bootstrap.

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).


Compatibility
==============

hemis Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.8+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

hemis Core should also work on most other Unix-like systems but is not frequently tested on them.

### :exclamation::exclamation::exclamation: MacOS 10.13 High Sierra :exclamation::exclamation::exclamation:

**Currently there are issues with the 3.x gitian release on MacOS version 10.13 (High Sierra), no reports of issues on older versions of MacOS.**

 
Notable Changes
==============

Budget Amount Calculation
--------------

A commit from our private repository that changed the available budget amount was inadvertently missed when merging back to the public repository. Thanks goes to community member Ronan for pointing this out.

Windows gamemaster.conf bug
--------------

Windows Gamemaster Controller wallets were experiencing an issue with reading the `gamemaster.conf` file during startup that prevented the wallet from opening properly. 

Windows file icons
-------------

The Windows program icons and installer images were of less than ideal quality, often appearing grainy or distorted. These icons/images have now been recreated to meet higher standards.

*3.1.0.2* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### P2P Network
- #592 `39430995c` Update protocol to 70914. (presstab)

### Budget
- #590 `413fad929` [Budget] Fix wrong budget amount (Mrs-X)
- #591 `405612f3c` Add unit test for budget value. (presstab)

### Miscellaneous
- #586 `fc211bfdf` [Bug] Fix CGamemasterConfig::read (Fuzzbawls)
- #587 `69498104f` [Bug] Fix Windows icon files (Fuzzbawls)

## Credits

Thanks to everyone who directly contributed to this release:
- Fuzzbawls
- Mrs-X
- presstab

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.
