hemis Core version 3.0.6 is now available from:

  <https://github.com/hemis-project/hemis/releases>

This is a new minor-revision version release, including various bug fixes and
performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/hemis-project/hemis/issues>


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

**Currently there are issues with the 3.0.x gitian releases on MacOS version 10.13 (High Sierra), no reports of issues on older versions of MacOS. As such, a High Sierra Only version is included below.**


Notable Changes
===============

Automated Database Corruption Repair
---------------------
There have been cases of blockchain database corruption that can occur when hemis client is not closed gracefully. The most common cases of corruption have been identified and the wallet will now automatically fix most of these corruptions. Certain corruption states are still unable to be fixed, but now provide more detailed error messages to the user as well as prompting the user to reindex their database.

More Accurate Error Messages
---------------------
Some error messages in the wallet have been too vague and done little to help developers and the support team properly identify issues. Error messages have been refined and are now more specific.

Reduction of Debug Log Spam
---------------------
Many 3rd party services have reported that their debug logs have been overloaded with messages about unknown transaction types. This log spam has been fixed.

Removal of Heavy Running Transaction Search Code
---------------------
Many areas of the block validation code use a "slow" transaction search, which searches redundantly for transactions. This "slow" search has been removed upstream in Bitcoin and is now removed in hemis. This provides a more efficient syncing process and generally better performing wallet.

Sync Fix for Block 908000
---------------------
Many wallets were having trouble getting past block 908000. This block recalculates certain aspects of the money supply and zHMS transactions, and is known to take longer to sync. Code has been added to allow block 908000 to be validated without the user needing to enter any special commands into the debug console.

Working Testnet
---------------------
Testnet is now accessible with this release of the wallet. Testnet can be accessed using the `-testnet` startup flag.

zHMS Spending Fix
---------------------
zHMS that were minted between block 891730 and 895400 were experiencing an error initializing the accumulator witness data correctly, causing an inability to spend those mints. This has been fixed.


3.0.6 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned.

### RPC and other APIs
- #366 `f361344` Do not stringify beyond OP_ZEROCOINSPEND for CScript::ToString() (presstab)
- #385 `732bfb0` getspentzerocoinamount RPC call. (whateverpal)
- #417 `f97b409` [RPC] Fix typo for `obfuscation` RPC command (Patrick Collins)

### Block and Transaction Handling
- #395 `5c5a9c6` [Main] Avoid slow transaction search with txindex enabled (Fuzzbawls)
- #405 `e415420` [Main] Automate database corruption fix caused by out of sync txdb. (presstab)
- #408 `beae959` Fix "accumulator does not verify" when spending zHMS. (presstab)
- #418 `90b0310` Fix edge case segfault. (presstab)

### P2P Protocol and Network Code
- #393 `58ec23f` [Testnet] Adjust testnet chainparams to new hard coded values. (presstab)

### Wallet
- #412 `2fb5f17` Double check tx size when creating zHMS tx's. (presstab)

### GUI
- #384 `7897f60` [Qt] Periodic make translate (Fuzzbawls)

### Build System
- #402 `e383b94` Change git info in genbuild.sh (Jon Spock)
- #419 `79956d4` [Travis] Add logprint-scanner.py to TravisCI (Fuzzbawls)

### Miscellaneous
- #401 `f30d9b7` [Scripts] LogPrint(f) scanner script (Sonic, PeterL73)
- #409 `4f78e67` Handle debug.log "CWalletTx::GetAmounts: Unknown transaction type" spam. (presstab)

Credits
=======

Thanks to everyone who directly contributed to this release:
- Fuzzbawls
- Jon Spock
- Mrs-X
- Patrick Collins
- PeterL73
- presstab
- sonic
- whateverpal

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/).
