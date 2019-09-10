PIVX Core version *3.4.0* is now available from:  <https://github.com/pivx-project/pivx/releases>

This is a new major version release, including various bug fixes and performance improvements.

Please report bugs using the issue tracker at github: <https://github.com/pivx-project/pivx/issues>


Mandatory Update
==============

PIVX Core v3.4.0 is a mandatory update for all users. This release contains new consensus rules and improvements that are not backwards compatible with older versions. Users will need to update their clients before enforcement of this update goes into effect.

Update enforcement goes into effect at the following times:

    Testnet: Tuesday, August 27, 2019 7:00:00 PM GMT
    Mainnet: Friday, August 30, 2019 4:00:00 PM GMT

Masternodes will need to be restarted once both the masternode daemon and the controller wallet have been upgraded.

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/PIVX-Qt (on Mac) or pivxd/pivx-qt (on Linux).


Compatibility
==============

PIVX Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.10+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

Apple released it's last Mountain Lion update August 13, 2015, and officially ended support on [December 14, 2015](http://news.fnal.gov/2015/10/mac-os-x-mountain-lion-10-8-end-of-life-december-14/). PIVX Core software starting with v3.2.0 will no longer run on MacOS versions prior to Yosemite (10.10). Please do not report issues about MacOS versions prior to Yosemite to the issue tracker.

PIVX Core should also work on most other Unix-like systems but is not frequently tested on them.


Notable Changes
==============

## Internal (Core) Changes

### Version 2 Stake Modifier

A new 256-bit modifier for the proof of stake protocol has been defined, `CBlockIndex::nStakeModifierV2`.
It is computed at every block, by taking the hash of the modifier of previous block along with the coinstake input.
To meet the protocol, the PoS kernel must comprise the modifier of the previous block.

Changeover enforcement of this new modifier is set to occur at block `1214000` for testnet and block `1967000` for mainnet.

### Block index batch writing

Block index writes are now done in a batch. This allows for less frequent disk access, meaning improved performances and less data corruption risks.

### Eliminate needless key generation

The staking process has been improved to no longer request a new (unused) key from the keypool. This should reduce wallet file size bloat as well as slightly improve staking efficiency.

### Fix crash scenario at wallet startup

A program crash bug that happens when the wallet.dat file contains a zc public spend transaction (input) and the user had removed the chain data has been fixed.

## GUI Changes

### Removal of zero-fee transaction option

The long term viability of acceptable zero-fee transaction conditions is in need of review. As such, we are temporarily disabling the ability to create zero-fee transactions.

### Show latest block hash and datadir information tab

A QoL addition has been made to the Information tab of the UI's console window, which adds the display of both the current data directory and the latest block hash seen by the client.

## RPC Changes

### Require valid URL scheme when preparing/submitting a proposal

The `preparebudget` and `submitbudget` RPC commands now require the inclusion of a canonical URL scheme as part of their `url` parameter. Strings that don't include either `http://` or `https://` will be rejected.

The 64 character limit for the `url` field is inclusive of this change, so the use of a URL shortening service may be needed.

## Testing Suite Changes

### Functional testing readability

Several changes have been introduced to the travis script in order to make the output more readable. Specifically it now lists tests left to run and prints the output of failing scripts.

## Build System Changes

### OpenSSL configure information

When the configure step fails because of an unsupported OpenSSL (or other library), it now displays more information on using an override flag to compile anyways. The long term plan is to ensure that the consensus code doesn't depend on OpenSSL in any way and then remove this configure step and related override flag.

*3.4.0* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core Features
 - #983 `ac8cb7376d` [PoS] Stake Modifier V2 (random-zebra)
 - #958 `454c487424` [Staking] Modify miner and staking thread for efficiency (Cave Spectre)
 - #915 `9c5a300624` Modify GetNextWorkRequired to set Target Limit correctly (Cave Spectre)
 - #952 `7ab673f6fa` [Staking] Prevent potential negative out values during stake splitting (Cave Spectre)
 - #941 `0ac0116ae4` [Refactor] Move ThreadStakeMinter out of net.cpp (Fuzzbawls)
 - #932 `924ec4f6dd` [Node] Do all block index writes in a batch (Pieter Wuille)

### Build System
 - #934 `92aa6c2daa` [Build] Bump master to 3.3.99 (pre-3.4) (Fuzzbawls)
 - #943 `918852cb90` [Travis] Show functional tests progress (warrows)
 - #957 `2c9f624455` [Build] Add info about '--with-unsupported-ssl' (Warrows)

### P2P Protocol and Network Code
 - #987 `fa1dbab247` [Net] Protocol update enforcement for 70917 and new spork keys (Fuzzbawls)

### GUI
 - #933 `e47fe3d379` [Qt] Add blockhash + datadir to information tab (Mrs-X)

### RPC/REST
 - #950 `3d7e16e753` [RPC] require valid URL scheme on budget commands (Cave Spectre)
 - #964 `a03fa6236d` [Refactor] Combine parameter checking of budget commands (Cave Spectre)
 - #965 `b9ce433bd5` [RPC] Correct issues with budget commands (Cave Spectre)

### Wallet
 - #939 `37ad934ad8` [Wallet] Remove (explicitely) unused tx comparator (warrows)
 - #971 `bbeabc4d63` [Wallet][zPIV] zc public spend parse crash in wallet startup. (furszy)
 - #980 `8b81d8f6f9` [Wallet] Remove Bitcoin Core 0.8 block hardlinking (JSKitty)
 - #982 `a0a1af9f78` [Miner] Don't create new keys when generating PoS blocks (random-zebra)

### Test Suites
 - #961 `2269f10fd9` [Trivial][Tests] Do not fail test when warnings are written to stderr (random-zebra)
 - #974 `f9d4ee0b15` [Tests] Add Spork functional test and update RegTest spork key (random-zebra)
 - #976 `12de5ec1dc` [Refactor] Fix stake age checks for regtest (random-zebra)

### Miscellaneous
 - #947 `6ce55eec2d` [Scripts] Sync github-merge.py with upstream (Fuzzbawls)
 - #948 `4a2b4831a9` [Docs] Clean and re-structure the gitian-keys directory (Fuzzbawls)
 - #949 `9e4c3576af` [Refactor] Remove all "using namespace" statements (warrows)
 - #951 `fa40040f80` [Trivial] typo fixes (Cave Spectre)
 - #986 `fdd0cdb72f` [Doc] Release notes update (Fuzzbawls)


## Credits

Thanks to everyone who directly contributed to this release:
- Cave Spectre
- Chun Kuan Lee
- Fuzzbawls
- Isidoro Ghezzi
- JSKitty
- MarcoFalke
- Mrs-X
- Pieter Wuille
- Steven Roose
- Warrows
- furszy
- random-zebra

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/pivx-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.