hemis Core version 3.0.2 is now available from:

  <https://github.com/hemis-project/hemis/releases>

This is a new minor-revision version release, including various bug fixes and
performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/hemis-project/hemis/issues>

Recommended Update
==============

hemis Core v3.0.2 is a recommended, semi-mandatory update for all users. This release contains transaction creation bug fixes for zHMS spends, automint calculation adjustments, and other various updates/fixes.

zHMS spending requires this update.

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

**Currently there are issues with the 3.0.x gitian releases on MacOS version 10.13 (High Sierra), no reports of issues on older versions of MacOS.**


Notable Changes
===============

Auto Wallet Backup
---------------------
In addition to the automatic wallet backup that is done at each start of the client, a new automatic backup function has been added that will, by default, create a backup of the wallet file during each zHMS mint operation (zHMS spends which re-mint their change are also included in this). This functionality is controlled by the `-backupzpiv` command-line option, which defaults to `1` (enabled, auto-backup).

Users that wish to prevent this behavior (not recommended) can pass `-backupzpiv=0` at the command-line when starting the client, or add `backupzpiv=0` to their `hemis.conf` file.

zHMS Automint Calculations
---------------------
A bug in the automint calculations was made apparent on mainnet when block times exceeded expectations, resulting in zHMS mint transactions that were in an unconfirmed state to still be treated as if they had never been minted. This caused automint to effectively mint more than what was intended.

zHMS Spending Fix
---------------------
The size of zHMS spend transactions is knowingly larger than normal transactions, and while this was expected, a much stricter check against the scriptsig size is used for mainnet, causing the transactions to be rejected by the mempool, and thus not being packaged into any blocks.

zHMS Transaction Recovery
---------------------
Due to the aforementioned issue with zHMS spending, users may find that their attempted spends are now conflicted and zHMS balances are not represented as expected. "Recovery" of these transactions can be done using the following methods:

1. GUI:

   The Privacy tab has the `Reset` and `Rescan` buttons that can be used to restore these mints/spends from a state of being marked as unavailable.

2. RPC:

   The RPC commands `resetspentzerocoin` and `resetmintzerocoin` are the command-line counterparts to the above, and can be used by users that do not use the GUI wallet.

RPC Changes
---------------------
The `bip38decrypt` command has had it's parameter order changed to be more consistent with it's counterpart. The command now expects the hemis address as it's first parameter and the passphrase as it's second parameter.

Bip38 Compatibility With 3rd Party Tools
---------------------
The in-wallet bip38 encryption method was leaving the final 4 bytes of the encrypted key blank. This caused an incompatibility issue with 3rd party tools like the paper wallet generators that could decrypt bip38 encrypted keys. Cross-tool compatibility has now been restored.

3.0.2 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned.

### RPC and other APIs
- #275 `059aaa9` [RPC] Change Parameter Order of bip38decrypt (presstab)

### P2P Protocol and Network Code
- #286 `85c0f53` [Main] Change sporkDB from smart ptr to ptr. (presstab)
- #292 `feadab4` Additional checks for double spending of zPiv serials. (presstab)

### Wallet
- #271 `5e9a086` [Wallet] Remove unused member wallet in UnlockContext inner class (Jon Spock)
- #279 `e734010` Add -backupzpiv startup flag. (presstab)
- #280 `fdc182d` [Wallet] Fix zPiv spending errors. (presstab)
- #282 `310f216` [Wallet] Count pending zPiv balance for automint. (presstab)
- #290 `004d7b6` Include both pending and mature zerocoins for automint calculations (presstab)

### GUI
- #268 `bc63f24` [GUI/RPC] Changed bubblehelp text + RPC startgamemaster help text fixed (Mrs-X)
- #269 `5466a9b` Check if model is valid before using in transactionView (Jon Spock)
- #270 `bd2328e` [Qt] Make lock icon clickable to toggle wallet lock state (Fuzzbawls)
- #273 `f31136e` [Qt] Fix UI tab order and shortcuts (Mrs-X)
- #287 `74a1c3c` [Qt] Don't allow the Esc key to close the privacy tab (Fuzzbawls)
- #291 `cb314e6` [Qt] zPiv control quantity/amount fixes (rejectedpromise)

### Miscellaneous
- #266 `2d97b54` [Scripts] Fix location for aarch64 outputs in gitian-build.sh (Fuzzbawls)
- #272 `958f51e` [Minting] Replace deprecated auto_ptr. (presstab)
- #276 `03f14ba` Append BIP38 encrypted key with an 4 byte Base58 Checksum (presstab)
- #288 `2522aa1` Bad CBlockHeader copy. (furszy)

Credits
=======

Thanks to everyone who directly contributed to this release:
- Fuzzbawls
- Jon Spock
- Mrs-X
- furszy
- presstab
- rejectedpromise
- Warrows

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/).
