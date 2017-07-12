PIVX Core version 2.2.1 is now available from:

  <https://github.com/pivx-project/pivx/releases>

This is a new minor version release, including various bug fixes and
performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/pivx-project/pivx/issues>

Compatibility
==============

PIVX Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support),
No attempt is made to prevent installing or running the software on Windows XP, you
can still do so at your own risk but be aware that there are known instabilities and issues.
Please do not report issues about Windows XP to the issue tracker.

PIVX Core should also work on most other Unix-like systems but is not
frequently tested on them.

Notable Changes
===============

Block Data Corruption
---------------------

Additional startup procedures have been added to fix corrupted blockchain databases.
The majority of users that are experiencing #106 (ConnectBlock() assertion on startup)
that have tested the new wallet have reported that their corrupt blockchain has
successfully been repaired. The new code will automatically detect and repair the
blockchain if it is able to.

If users still experience corruptions with the new wallet and it is not fixed
with the new startup procedures, it is suggested that they try using the
`-forcestart` startup flag which will bypass the new procedures altogether, and
in rare cases allow the wallet to run. If the database is not fixed by either
the automatic procedures or the `-forcestart` flag, the user should resync the
blockchain.

Additional progress has been made to prevent the wallet crashes that are causing
the corrupted databases, for example removing the Trading Window (explained below)
and fixing several other minor memory leaks that were inherited from the version
of Bitcoin that PIVX was forked from.

RPC Changes
-----------

- Exporting or dumping an addresses' private key while the wallet is unlocked for
  anonymization and Staking only is no longer possible.

- A new command (`getstakingstatus`) has been added that returns the internal conditions
  for staking to be activated and their status.

- KeePass integration has been removed for the time being due to various inefficiencies
  with it's code.

Trading Window Removed
----------------------

The Bittrex trading window in the GUI wallet was problematic with it's memory
handling, often leaking, and was overall an inefficient use of resources in it's
current implementation. A revised multi-exchange trading window may be implemented
at a later date.

2.2.1 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned.

### RPC and other APIs
- #130 `ccb1526` [RPC] Add `getstakingstatus` method
- #138 `4319af3` [RPC] Require password when using UnlockAnonymizeOnly
- #142 `6b5cf7f` [RPC] Remove Keepass code due to Valgrind warnings

### Block and Transaction Handling
- #146 `bce67cb` [Wallet] Look at last CoinsView block for corruption fix process
- #154 `1b3c0d7` [Consensus] Don't pass the genesis block through CheckWork

### P2P Protocol and Network Code
- #168 `ac912d9` [Wallet] Update checkpoints with v2.2 chain
- #162 `0c0d080` Remove legacy Dash code IsReferenceNode
- #163 `96b8b00` [P2P] Change alert key to effectively disable it

### GUI
- #131 `238977b` [Qt] Adds base CSS styles for various elements
- #134 `f7cabbe` [Qt] Edit masternode.conf in Qt-wallet
- #135 `f8f1904` [Qt] Show path to wallet.dat in wallet-repair tab
- #136 `53705f1` [Qt] Fix false flags for MultiSend notification when sending transactions
- #137 `ad08051` [Qt] Fix Overview Page Balances when receiving
- #141 `17a9e0f` [Qt] Squashed trading removal code
- #151 `0409b12` [Qt] Avoid OpenSSL certstore-related memory leak
- #165 `0dad320` [Qt] More place for long locales

### Miscellaneous
- #133 `fceb421` [Docs] Add GitHub Issue template and Contributor guidelines
- #144 `e4e68bc` [Wallet] Reduce usage of atoi to comply with CWE-190
- #152 `6a1de07` [Trivial] Use LogPrint for repetitive budget logs
- #157 `41fdeaa` [Budget] Add log for removed budget proposals
- #166 `d37b4aa` [Utils] Add ExecStop= to example systemd service
- #167 `a6becee` [Utils] makeseeds script update

Credits
=======

Thanks to everyone who directly contributed to this release:

- Aaron Miller
- Fuzzbawls
- Mrs-X
- Spock
- presstab

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/pivx-project-translations/).
