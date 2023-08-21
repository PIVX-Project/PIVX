hemis Core version *3.1.1* is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new minor version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>

Non-Mandatory Update
==============

hemis Core v3.1.1 is a non-mandatory update to address bugs and introduce minor enhancements that do not require a network change.

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).


Compatibility
==============

hemis Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support),
No attempt is made to prevent installing or running the software on Windows XP, you
can still do so at your own risk but be aware that there are known instabilities and issues.
Please do not report issues about Windows XP to the issue tracker.

hemis Core should also work on most other Unix-like systems but is not
frequently tested on them.

### :exclamation::exclamation::exclamation: MacOS 10.13 High Sierra :exclamation::exclamation::exclamation:

**Currently there are issues with the 3.0.0+ gitian release on MacOS version 10.13 (High Sierra), no reports of issues on older versions of MacOS.**

Notable Changes
==============

zHMS Updates
--------------

### Fix spending for v1 zHMS created before block 1050020

The transition to v2 zHMS and reset of the accumulators caused blocks 1050000 - 1050010 to be accumulated twice. This was causing a number v1 zHMS to not create valid witnesses, and thus were not spendable. This problem is fixed by double accumulating blocks 1050000-1050010 when creating the witness. Any user that had issues spending zHMS v1 will now be able to convert that into HMS and then zHMS v2 (if desired).

### Adjustment to staking properties to reduce orphaned blocks

zHMS stake set to update more frequently and lowering the stake hashdrift to 30 seconds to reduce the number of orphans being experienced by hemis stakers.

Further work is being done to improve the efficiently of zPoS beyond this, and will be available in a subsequent release at a later date.


User Experience
--------------

### Fix wrongly displayed balance on Overview tab

Fixes a display issue introduced with a previous change. This was a "display only" issue, all your coins were there all the time.

### Show progress percent for zpiv reindex operations

When starting the wallet with `-reindexaccumulators` and/or `-reindexzerocoin`, these operations can take a considerable time to complete depending on system hardware. A progress percent on the splash screen is now shown for these processes to avoid confusion in thinking that the wallet has frozen.


### Add TOR service icon to status bar

An icon is now shown for clients that are connected and operating over the TOR network. Included is a mouse-over tooltip showing the onion address associated with the client. This icon is only shown when a connection to the TOR network can be established, and will be hidden otherwise.


hemis Daemon & Client (RPC Changes)
--------------

### Fix listtransactions RPC function

This addresses an issue where new incoming transactions are not recorded properly, and subsequently, not returned with `listtransactions` in the same session.

This fix was previously included in the `v3.1.0.3` tag, and relayed to affected exchanges/services, which typically use this command for accounting purposes. It is included here for completeness.

Technical Changes
--------------

### Switch to libsecp256k1 signature verification

Here is the long overdue update for hemis to let go of OpenSSL in its consensus code. The rationale behind it is to avoid depending on an external and changing library where our consensus code is affected. This is security and consensus critical. hemis users will experience quicker block validations and sync times as block transactions are verified under libsecp256k1.

The recent [CVE-2018-0495](https://www.nccgroup.trust/us/our-research/technical-advisory-return-of-the-hidden-number-problem/) brings into question a potential vulnerability with OpenSSL (and other crypto libraries) that libsecp256k1 is not susceptible to.

### Write to the zerocoinDB in batches

Instead of using a separate write operation for each and every bit of data that needs to be flushed to disk, utilize leveldb's batch writing capability. The primary area of improvement this offers is when reindexing the zerocoinDB (`-reindexzerocoin`), which went from needing multiple hours on some systems to mere minutes.

Secondary improvement area is in ConnectBlock() when multiple zerocoin transactions are involved.

### Resolution of excessive peer banning

It was found that following a forced closure of the hemis core wallet (ungraceful), a situation could arise that left partial/incomplete data in the disk cache. This caused the client to fail a basic sanity test and ban any peer which was sending the (complete) data. This, in turn, was causing the wallet to become stuck. This issue has been resolved client side by guarding against this partial/incomplete data in the disk cache.

*3.1.1* Change log
--------------

Detailed release notes follow. This overview includes changes that affect behavior, code moves, refactoring and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core Features
 - #549 `8bf13a5ad` [Crypto] Switch to libsecp256k1 signature verification and update the lib (warrows)
 - #609 `6b73598b9` [MoveOnly] Remove zHMS code from main.cpp (presstab)
 - #610 `6c3bc8c76` [Main] Check whether tx is in chain in ContextualCheckZerocoinMint(). (presstab)
 - #624 `1a82aec96` [Core] Missing seesaw value for block 325000 (warrows)
 - #636 `d359c6136` [Main] Write to the zerocoinDB in batches (Fuzzbawls)

### Build System
 - #605 `b4d82c944` [Build] Remove unnecessary BOOST dependency (Mrs-X)
 - #622 `b8c672c98` [Build] Make sure Boost headers are included for libzerocoin (Fuzzbawls)
 - #639 `98c7a4f65` [Travis] Add separate job to check doc/logprint/subtree (Fuzzbawls)
 - #648 `9950fce59` [Depends] Update Qt download url (fanquake)
 
### P2P Protocol and Network Code
 - #608 `a602d00eb` [Budget] Make sorting of finalized budgets deterministic (Mrs-X)
 - #647 `3aa3e5c97` [Net] Update hard-coded fallback seeds (Fuzzbawls)

### GUI
 - #580 `c296b7572` Fixed Multisend dialog to show settings properly (SHTDJ)
 - #598 `f0d894253` [GUI] Fix wrongly displayed balance on Overview tab (Mrs-X)
 - #600 `217433561` [GUI] Only enable/disable PrivacyDialog zHMS elements if needed. (presstab)
 - #612 `6dd752cb5` [Qt] Show progress percent for zpiv reindex operations (Fuzzbawls)
 - #626 `9b6a42ba0` [Qt] Add Tor service icon to status bar (Fuzzbawls)
 - #629 `14e125795` [Qt] Remove useless help button from QT dialogs (windows) (warrows)
 - #646 `c66b7b632` [Qt] Periodic translation update (Fuzzbawls)
 
### Wallet
 - #597 `766d5196c` [Wallet] Write new transactions to wtxOrdered properly (Fuzzbawls)
 - #603 `779d8d597` Fix spending for v1 zHMS created before block 1050020. (presstab)
 - #617 `6b525f0df` [Wallet] Adjust staking properties to lower orphan rates. (presstab)
 - #625 `5f2e61d60` [Wallet] Add some LOCK to avoid crash (warrows)
 
### Miscellaneous
 - #585 `76c01a560` [Doc] Change aarch assert sign output folder (Warrows)
 - #595 `d2ce04cc0` [Tests] Fix chain ordering in budget tests (Fuzzbawls)
 - #611 `c6a57f664` [Output] Properly log reason(s) for increasing a peer's DoS score. (Fuzzbawls)
 - #649 `f6bfb4ade` [Utils] Add copyright header to logprint-scanner.py (Fuzzbawls)
 
## Credits
Thanks to everyone who directly contributed to this release:

 - Fuzzbawls
 - Mrs-X
 - SHTDJ
 - Sieres
 - Warrows
 - fanquake
 - gpdionisio
 - presstab


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/).
