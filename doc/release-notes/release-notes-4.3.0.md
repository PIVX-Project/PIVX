hemis Core version *4.3.0* is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>


Recommended Update
==============

This version is an optional, but recommended, update for all users and services.

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).

Downgrading warning
-------------------

The chainstate database for this release is not compatible with previous releases, so if you run 4.3.0 and then decide to switch back to any older version, you will need to run the old release with the -reindex option to rebuild the chainstate data structures in the old format.


Compatibility
==============

hemis Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.10+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

Apple released it's last Mountain Lion update August 13, 2015, and officially ended support on [December 14, 2015](http://news.fnal.gov/2015/10/mac-os-x-mountain-lion-10-8-end-of-life-december-14/). hemis Core software starting with v3.2.0 will no longer run on MacOS versions prior to Yosemite (10.10). Please do not report issues about MacOS versions prior to Yosemite to the issue tracker.

hemis Core should also work on most other Unix-like systems but is not frequently tested on them.


Notable Changes
==============

Performance Improvements
------------------------

Version 4.3.0 contains a number of significant performance improvements, which make Initial Block Download, startup, transaction and block validation much faster:

- The chainstate database (which is used for tracking UTXOs) has been changed from a per-transaction model to a per-output model ([See PR 1801](https://github.com/hemis-Project/hemis/pull/1801)). Advantages of this model are that it:
  - avoids the CPU overhead of deserializing and serializing the unused outputs;
  - has more predictable memory usage;
  - uses simpler code;
  - is adaptable to various future cache flushing strategies.
  
  As a result, validating the blockchain during Initial Block Download (IBD) and reindex is ~30-40% faster, uses 10-20% less memory, and flushes to disk far less frequently. The only downside is that the on-disk database is 15% larger. During the conversion from the previous format a few extra gigabytes may be used.

- LevelDB has been upgraded to version 1.22 ([See PR 1738](https://github.com/hemis-Project/hemis/pull/1738)). This version contains hardware acceleration for CRC on architectures supporting SSE 4.2. As a result, synchronization and block validation are now faster.

Removal of Priority Estimation
------------------------------

Estimation of "priority" needed for a transaction to be included within a target number of blocks has been removed.  The rpc calls are deprecated and will either return `-1` or `1e24` appropriately. 

The format for fee_estimates.dat has also changed to no longer save these priority estimates. It will automatically be converted to the new format which is not readable by prior versions of the software.

Dedicated mnping logging category
---------------------------------

`mnping` related debug log messages have been moved to their own category of the same name. This is to reduce log spam when debugging with the `masternode` category enabled.

RPC Changes
------------

### Modified input/output for existing commands

- The new database model no longer stores information about transaction
  versions of unspent outputs. This means that:
  - The `gettxout` RPC no longer has a `version` field in the response.
  - The `gettxoutsetinfo` RPC reports `hash_serialized_2` instead of `hash_serialized`,
    which does not commit to the transaction versions of unspent outputs, but does
    commit to the height and coinbase/coinstake information.
  - The `getutxos` REST path no longer reports the `txvers` field in JSON format,
    and always reports 0 for transaction versions in the binary format
- Three filtering options for the `getbalance` command have been reinstated:
  - `minconf` (numeric) Only include transactions confirmed at least this many times.
  - `includeWatchonly` (bool) Also include balance in watchonly addresses.
  - `includeDelegated` (bool) Also include balance delegated to cold stakers.
- `estimatefee` is now deprecated and replaced by `estimatesmartfee`:
  - Input argument is the same for `estimatesmartfee`.
  - Output is now a JSON object with 2 fields: `feerate` and `blocks`
- The `getrawmempool` RPC command now includes an additional output field:
  - `modifiedfee` (numeric) transaction fee with fee deltas used for mining priority.::ZZZZexit
  

### Removed commands

The following commands have been removed from the interface:
- `estimatepriority`

*4.3.0* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core Features
 - #1666 `5a092159f6` [Core] Base work for the Sapling signatureHash (furszy)
 - #1746 `128978d45b` [Core] Only include undo.h from main.cpp (random-zebra)
 - #1768 `6881e1063f` [Core] Use SipHash-2-4 for various non-cryptographic hashes (Pieter Wuille)
 - #1771 `bda654c5f3` [Core] Use SipHash for node eviction (Pieter Wuille)
 - #1773 `90ffc6683b` [Core] per-txout model preparation (random-zebra)
 - #1774 `68df9a7d5b` [Core] Alter assumptions in CCoinsViewCache::BatchWrite (random-zebra)
 - #1775 `85b5f2eb83` [Core] Remove BIP30 check (random-zebra)
 - #1777 `3c767c46b5` [Core] ModifyNewCoins saves database lookups (random-zebra)
 - #1788 `823ba8e334` [Core] Remove priority estimation (random-zebra)
 - #1793 `af793b7bb9` [Core] Safer modify new coins (Russell Yanofsky)
 - #1795 `afafd7f6a9` [Core] Use unordered_map for CCoinsMap and fix empty vectors in streams (Pieter Wuille)
 - #1799 `fcb546ad05` [Core] Remove UTXO cache entries when the tx they were added for is removed (Pieter Wuille)
 - #1801 `30d353edab` [Core] Per-txout model for chainstate database (random-zebra)
 - #1804 `5c8b992033` [Core] Use std::unordered_{map,set} (C++11) instead of boost::unordered_* (random-zebra)

### GUI
 - #1754 `93d574170d` [Model] Wallet interface refactor + code cleanup. (furszy)
 - #1776 `2ad27b1407` [Model] TransactionRecord decomposeTransaction refactoring (furszy)
 - #1782 `ada4462782` [GUI] Start masternodes polling timer only when is needed. (furszy)
 - #1805 `f0cc6fcc38` [BUG][GUI] Don't append cold-stake records twice (random-zebra)
 - #1863 `ad15bce2f5` [Trivial][GUI] Fix init messages (random-zebra)

### Wallet
 - #1752 `2e32285a70` [Wallet] Simple unused methods cleanup. (furszy)
 - #1755 `eeb129b477` [wallet] List COutput solvability + wallet_ismine refactoring. (furszy)
 - #1757 `e2cc4aa411` [Wallet] add cacheable amounts for caching credit/debit values (furszy)
 - #1759 `dcc92f8157` [Wallet] AvailableCoins remove duplicated watchonly config argument. (furszy)
 - #1760 `3b030f9978` [Wallet] AvailableCoins code readability improved (furszy)
 - #1764 `6847d0d648` [Wallet] Securely erase potentially sensitive keys/values (Thomas Snider)
 - #1767 `8ab63d3e5b` [Wallet] Ignore MarkConflict if block hash is not known (random-zebra)
 - #1781 `4715915d4c` [Wallet] Acquire cs_main lock before cs_wallet during wallet initialization (random-zebra)
 - #1783 `abf7c62934` [Wallet] Do not try to resend transactions if the node is importing or in IBD (furszy)
 - #1787 `4b1f3eb792` [Wallet] Improve usage of fee estimation code (random-zebra)
 - #1802 `7db7724cff` [Wallet] Make nWalletDBUpdated atomic to avoid a potential race (furszy)
 - #1810 `49bd99929d` [Wallet] wtx cached balances test coverage + getAvailableCredit problem fix. (furszy)
 - #1811 `e89e20eca1` [Wallet][Refactoring] wallet/init refactoring backports (random-zebra)
 - #1817 `6480c7d9bf` [Wallet] Speedup coinstake creation removing redundancies. (furszy)
 - #1832 `c14d130b48` [Wallet] Cleanup getbalance methods that are not fulfilling any purpose. (furszy)

### P2P Protocol and Network Code
 - #1769 `1e334200bb` [Net] Remove bogus assert on number of oubound connections. (Matt Corallo)
 - #1780 `5fcad0c139` [Net] cs_vSend-cs_main deadlock detection fixed. (furszy)
 - #1800 `616b102f8b` [P2P] Improve AlreadyHave (Alex Morcos)
 - #1812 `777638e7bc` [P2P] Begin Network Encapsulation (random-zebra)
 - #1835 `cbd9271afb` [Net] Massive network refactoring and speedup (Fuzzbawls)

### RPC/REST
 - #1753 `e288a4508b` [Trivial] [RPC] Fix listcoldutxos help text (JSKitty)
 - #1828 `4fc36b59ee` [RPC][BUG] Fix ActivateBestChain calls in reconsider(invalidate)block (random-zebra)
 - #1831 `28509bf9e8` [RPC] re introducing filtering args in getbalance (furszy)

### Build Systems
 - #1553 `fddf765132` [Build] Sapling Foundations (Build System + ZIP32) (furszy)
 - #1703 `2e11030e8b` [Build] Require minimum boost version 1.57.0 (Fuzzbawls)
 - #1738 `21c467b1eb` [Build] Update leveldb to 1.22+ (Fuzzbawls)
 - #1750 `544e619ebe` [Trivial] openssl.org dependency download link changed (CryptoDev-Project)
 - #1770 `32a2e8a031` [Build] Bump minimum libc to 2.17 for release binaries (Fuzzbawls)
 - #1790 `6e7b9b2a82` [Build] Fix glibc compat (Fuzzbawls)
 - #1792 `a3da3aa9f5` [Build] allow for empty RUST_TARGET in offline builds (Fuzzbawls)
 - #1794 `760f426430` [Build] Use syslibs for nightly snap builds (Fuzzbawls)
 - #1813 `259523cdc2` [CMake] Define MAC_OSX for cmake builds on macOS (Fuzzbawls)
 - #1823 `d675fa3a1a` [Travis] Lower the build timeout for the functional tests job (random-zebra)

### Layer 2 (MN/Budget)
 - #1791 `6162df962b` [Masternodes] Missing cs main locks in CalculateScore and GetMasternodeInputAge (furszy)
 - #1803 `69ec4a3fdf` [Cleanup] masternode-budget tiny cleanup. (furszy)
 - #1825 `a06c0fd993` [MN] more cleanup over the tier two area. (furszy)
 - #1826 `961f5373bf` [Refactor] Masternode Budget first refactoring and cleanup (random-zebra)
 - #1843 `f4d5d34bed` [Bug] Update budget manager best height even if mnSync is incomplete (random-zebra)

### Miner/Block Generation
 - #1700 `40742084de` [Miner] Move coinbase & coinstake to P2PKH (furszy)
 - #1809 `959d707bc9` [Miner] decouple zHMS duplicated serials checks from CreateNewBlock (furszy)
 - #1816 `0fa40d7695` [Miner] Unifying the disperse coinbase tx flow + further clean up. (furszy)
 - #1818 `242356d012` [Miner] PoS process (furszy)

### Miscellaneous
 - #1694 `0604a98bd0` [Backport] Test LowS in standardness (furszy)
 - #1721 `8e19562dc4` [Validation] Reduce cs_main locks during ConnectTip/SyncWithWallets (furszy)
 - #1725 `e59d8e59fa` [Backport] mempool score index. (Alex Morcos)
 - #1735 `ee749c5b9c` [Validation] DisconnectBlock updates. (furszy)
 - #1785 `277b1114d9` [Bug] lock cs_main for Misbehaving (furszy)
 - #1796 `6d62df529b` [BUG] Properly copy fCoinStake memeber between CTxInUndo and CCoins (random-zebra)
 - #1797 `b909e96121` [Refactoring] Break circular dependency main ↔ txdb (random-zebra)
 - #1806 `b9f30f65f2` [Refactor] Cleanup access to chainActive in a few places (random-zebra)
 - #1808 `948e1a99c0` [Tests][Trivial] Remove mining in rpc_deprecated test (random-zebra)
 - #1820 `846dca7b83` [Cleanup] remove unneeded chainActive access. (furszy)
 - #1821 `4b3fb02dc3` [Cleanup] removing unused GetMasternodeByRank method (furszy)
 - #1822 `48d7475bd4` [Refactor] Dedicated logging category for masternode pings (random-zebra)
 - #1824 `6ec609f93d` [Cleanup] IsBlockValueValid refactored properly. (furszy)
 - #1827 `70bf7203ee` [Cleanup] removed null check comparison against a new object. (furszy)
 - #1833 `9c06e5d7ce` [Refactor] Remove GetInputAge and GetMasternodeInputAge (random-zebra)
 - #1853 `e8d13ef4b0` [Cleanup] Removing unused and unneeded functions and members (furszy)
 - #1855 `3cd52771f2` [Bug] wrong reserveKey when committing budget/proposal collaterals (random-zebra)
 - #1860 `5aed03f6fe` [Bug] Missing mnping category added to logcategories (furszy)

## Credits

Thanks to everyone who directly contributed to this release:
- Alex Morcos
- Cory Fields
- CryptoDev-Project
- Daniel Kraft
- Ethan Heilman
- Fuzzbawls
- Gregory Maxwell
- JSKitty
- Jack Grigg
- Jeremy Rubin
- Luke Dashjr
- Matt Corallo
- Patrick Strateman
- Pavel Janík
- Philip Kaufmann
- Pieter Wuille
- Russell Yanofsky
- Suhas Daftuar
- Thomas Snider
- Wladimir J. van der Laan
- furszy
- jtimon
- random-zebra


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.