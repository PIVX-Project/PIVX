hemis Core version *4.2.0* is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>


Recommended Update
==============

This version is an optional, but recommended, update for all users and services.

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).


Compatibility
==============

hemis Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.10+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

Apple released it's last Mountain Lion update August 13, 2015, and officially ended support on [December 14, 2015](http://news.fnal.gov/2015/10/mac-os-x-mountain-lion-10-8-end-of-life-december-14/). hemis Core software starting with v3.2.0 will no longer run on MacOS versions prior to Yosemite (10.10). Please do not report issues about MacOS versions prior to Yosemite to the issue tracker.

hemis Core should also work on most other Unix-like systems but is not frequently tested on them.


Notable Changes
==============

### Removed zerocoin GUI

Spending zHMS and getting zHMS balance information is no longer available in the graphical interface ([#1549](https://github.com/hemis-Project/hemis/pull/1549)). The feature remains accessible through the RPC interface: `getzerocoinbalance`, `listmintedzerocoins`, `listzerocoinamounts`, `spendzerocoin`, `spendzerocoinmints`.


### Memory pool limiting

Previous versions of hemis Core had their mempool limited by checking a transaction's fees against the node's minimum relay fee. There was no upper bound on the size of the mempool and attackers could send a large number of transactions paying just slighly more than the default minimum relay fee to crash nodes with relatively low RAM.

hemis Core 4.2.0 will have a strict maximum size on the mempool. The default value is 300 MB and can be configured with the `-maxmempool` parameter. Whenever a transaction would cause the mempool to exceed its maximum size, the transaction that (along with in-mempool descendants) has the lowest total feerate (as a package) will be evicted and the node's effective minimum relay feerate will be increased to match this feerate plus the initial minimum relay feerate. The initial minimum relay feerate is set to 1000 satoshis per kB.

hemis Core 4.2.0 also introduces new default policy limits on the length and size of unconfirmed transaction chains that are allowed in the mempool (generally limiting the length of unconfirmed chains to 25 transactions, with a total size of 101 KB). These limits can be overridden using command line arguments ([#1645](https://github.com/hemis-Project/hemis/pull/1645), [#1647](https://github.com/hemis-Project/hemis/pull/1647)).

### Benchmarking Framework

hemis Core 4.2.0 backports  the internal benchmarking framework from Bitcoin Core, which can be used to benchmark cryptographic algorithms (e.g. SHA1, SHA256, SHA512, RIPEMD160, Poly1305, ChaCha20), Base58 encoding and decoding and thread queue. More tests are needed for script validation, coin selection and coins database, cuckoo cache, p2p throughtput ([#1650](https://github.com/hemis-Project/hemis/pull/1650)).

The binary file is compiled with hemis-core, unless configured with `--disable-bench`.<br>
After compiling hemis-core, the benchmarks can be run with:
```
src/bench/bench_hemis
```
The output will be similar to:
```
#Benchmark,count,min(ns),max(ns),average(ns),min_cycles,max_cycles,average_cycles
Base58CheckEncode,131072,7697,8065,7785,20015,20971,20242
```

'label' and 'account' APIs for wallet
-------------------------------------

A new 'label' API has been introduced for the wallet. This is intended as a
replacement for the deprecated 'account' API. The 'account' can continue to
be used in v4.2 by starting hemisd with the '-deprecatedrpc=accounts'
argument, and will be fully removed in v5.0.

The label RPC methods mirror the account functionality, with the following functional differences:

- Labels can be set on any address, not just receiving addresses. This functionality was previously only available through the GUI.
- Labels can be deleted by reassigning all addresses using the `setlabel` RPC method.
- There isn't support for sending transactions _from_ a label, or for determining which label a transaction was sent from.
- Labels do not have a balance.

Here are the changes to RPC methods:

| Deprecated Method       | New Method            | Notes       |
| :---------------------- | :-------------------- | :-----------|
| `getaccount`            | `getaddressinfo`      | `getaddressinfo` returns a json object with address information instead of just the name of the account as a string. |
| `getaccountaddress`     | n/a                   | There is no replacement for `getaccountaddress` since labels do not have an associated receive address. |
| `getaddressesbyaccount` | `getaddressesbylabel` | `getaddressesbylabel` returns a json object with the addresses as keys, instead of a list of strings. |
| `getreceivedbyaccount`  | `getreceivedbylabel`  | _no change in behavior_ |
| `listaccounts`          | `listlabels`          | `listlabels` does not return a balance or accept `minconf` and `watchonly` arguments. |
| `listreceivedbyaccount` | `listreceivedbylabel` | Both methods return new `label` fields, along with `account` fields for backward compatibility. |
| `move`                  | n/a                   | _no replacement_ |
| `sendfrom`              | n/a                   | _no replacement_ |
| `setaccount`            | `setlabel`            | Both methods now: <ul><li>allow assigning labels to any address, instead of raising an error if the address is not receiving address.<li>delete the previous label associated with an address when the final address using that label is reassigned to a different label, instead of making an implicit `getaccountaddress` call to ensure the previous label still has a receiving address. |

| Changed Method         | Notes   |
| :--------------------- | :------ |
| `listunspent`          | Returns new `label` fields, along with `account` fields for backward compatibility if running with the `-deprecatedrpc=accounts` argument |
| `sendmany`             | The first parameter has been renamed to `dummy`, and must be set to an empty string, unless running with the `-deprecatedrpc=accounts` argument (in which case functionality is unchanged). |
| `listtransactions`     | The first parameter has been renamed to `dummy`, and must be set to the string `*`, unless running with the `-deprecatedrpc=accounts` argument (in which case functionality is unchanged). |
| `getbalance`           | `account`, `minconf` and `include_watchonly` parameters are deprecated, and can only be used if running with the `-deprecatedrpc=accounts` argument |
| `getcoldstakingbalance`| The `account` parameter is deprecated, and can only be used if running with the `-deprecatedrpc=accounts` argument (in which case functionality is unchanged) |
| `getdelegatedbalance`  | The `account` parameter is deprecated, and can only be used if running with the `-deprecatedrpc=accounts` argument (in which case functionality is unchanged) |

GUI Changes
----------

### Topbar navigation

- The "sync" button in the GUI topbar can be clicked to go directly to the Settings --> Information panel (where the current block number and hash is shown).

- The "connections" button in the GUI topbar can be clicked to open the network monitor dialog ([#1688](https://github.com/hemis-Project/hemis/pull/1688)).

Functional Changes
----------

### Stake-Split threshold

If the stake split is active (threshold > 0), then stake split threshold value must be greater than a minimum, set by default at 100 HMS. The minimum value can be changed using the `-minstakesplit` startup flag ([#1586](https://github.com/hemis-Project/hemis/pull/1586)). A value `0` is still allowed, regardless of the minimum set, and, as before, can be used to disable the stake splitting functionality.

### Changed command-line options

- new command `-minstakesplit` to modify the minimum allowed for  the stake split threshold ([#1586](https://github.com/hemis-Project/hemis/pull/1586)).

- new commands `-maxmempool`, to customize  the memory pool size limit, and `-checkmempool=N`, to customize the frequency of the mempool check ([#1647](https://github.com/hemis-Project/hemis/pull/1647)).

- new commands `-limitancestorcount=N` and `limitancestorsize=N`, to limit the number and total size of all in-mempool ancestors for a transaction ([#1647](https://github.com/hemis-Project/hemis/pull/1647)).

- new commands `-limitdescendantcount=N` and `limitdescendantsize=N`, to limit the number and total size of all in-mempool descendants for a transaction ([#1647](https://github.com/hemis-Project/hemis/pull/1647)).

RPC Changes
------------

In addition to the afore mentioned 'label' and 'account' API changes, other RPC changes are as follows:

### Low-level API changes

- The `asm` property of each scriptSig now contains the decoded signature hash type for each signature that provides a valid defined hash type ([#1633](https://github.com/hemis-Project/hemis/pull/1633)).<br>
The following items contain assembly representations of scriptSig signatures
and are affected by this change: RPC `getrawtransaction`, RPC `decoderawtransaction`, REST `/rest/tx/` (JSON format), REST `/rest/block/` (JSON format when including extended tx details), `hemis-tx -json`

### Modified input/output for existing commands

- new "usage" field in the output of `getmempoolinfo`, displaying the total memory usage for the mempool ([#1645](https://github.com/hemis-Project/hemis/pull/1645)).

- new "upgrades" field in the output of `getblockchaininfo`, showing upcoming and active network upgrades ([#1665](https://github.com/hemis-Project/hemis/pull/1665), [#1687](https://github.com/hemis-Project/hemis/pull/1687)).

- `listreceivedbyaddress` has a new optional "addressFilter" argument that will filter the results to only the specified address

### Removed commands

- `gamemasterdebug`. Use `getgamemasterstatus` instead. ([#1698](https://github.com/hemis-Project/hemis/pull/1698)).

*4.2.0* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core Features
 - #1414 `e3ceda31c5` uint256 to arith_uint256 migration (Second step). (furszy)
 - #1486 `e146131780` Replace OpenSSL AES with ctaes-based version (furszy)
 - #1488 `cc84157e52` CCrypter using secure allocator vector. (furszy)
 - #1531 `1f9e7e45ba` [Backport] Implement accurate UTXO cache size accounting (furszy)
 - #1533 `898bcd24ad` [Backport] Make connect=0 disable automatic outbound connections. (furszy)
 - #1534 `d45c58e89d` [Backport] Preemptively catch a few potential bugs (furszy)
 - #1554 `0dedec8157` [Core] Big endian support (random-zebra)
 - #1557 `8dfc4806f7` [Core] Prevector (random-zebra)
 - #1564 `2c11e37c91` Abstract out CTransaction-specific signing into SignatureCreator (furszy)
 - #1587 `c9741c53b1` [Backport] Shut down if trying to connect a corrupted block (furszy)
 - #1593 `f59c8fb625` Prepare for non-Base58 addresses [Step 1] (furszy)
 - #1629 `73d26f20e9` [DB] Bitcoin 0.12-0.14 dbwrapper improvements (random-zebra)
 - #1630 `a9ac1cc51f` [Refactoring] Lightweight abstraction of boost::filesystem (random-zebra)
 - #1631 `56b414761c` [Miner] Remove 2017 invalid outputs check (furszy)
 - #1633 `8cd9c592a9` [Core] Bitcoin 0.12-0.14 serialization improvements (random-zebra)
 - #1634 `e0239f801d` [Core] prevector: named union and defaults vars initialization (random-zebra)
 - #1641 `7c102142c7` [Core] New fee estimation code (random-zebra)
 - #1645 `49867086da` [Core] Implement accurate memory accounting for mempool (random-zebra)
 - #1647 `eb00d0f62f` [Core] MemPool package tracking and limits (Alex Morcos)
 - #1650 `4ed15cc69d` [Tests] Benchmarking Framework (random-zebra)
 - #1665 `602ad3d7f8` New upgrade structure. (furszy)
 - #1668 `7e65a82e7d` [Backport] Make sigcache faster (furszy)
 - #1669 `c072659135` Better sigcache implementation (CuckooCache) (furszy)
 - #1670 `8d7e7808d0` [Refactoring] CValidationState logging (random-zebra)
 - #1673 `a5ba7200c6` [Core] Separate Consensus CheckTxInputs and GetSpendHeight in CheckInputs (furszy)
 - #1676 `961e2d2a70` [Backport] Base58 performance improvements (Kaz Wesley)
 - #1677 `95fefe9c0d` Prepare for non-Base58 addresses [Step 2] (furszy)
 - #1679 `c157deb76b` Prepare for non-Base58 addresses [Step 3] (furszy)
 - #1687 `f054e3e627` [Refactor] Network upgrades management cleanup (random-zebra)
 - #1697 `13576cfe14` [BUG] Fix mempool entry priority (furszy)
 - #1707 `6bc917e859` RPC & Mempool back ports. (furszy)
 - #1728 `1c472ebae8` IsInitialBlockDownload: usually avoid locking (furszy)
 - #1729 `76ea490ac1` [Core] Add checkpoints for hemis v4.1.1 enforcement (random-zebra)
 - #1733 `e4ae10db31` Move zerocoin validation to its own legacy file. (furszy)
 - #1747 `6f90e8be13` NU custom activation height startup arg. (furszy)

### Build System
 - #1681 `131ec069fd` [Build] Set complete cpp/cxx flags for bench_hemis binary (Fuzzbawls)
 - #1684 `69fa8dce5e` [Travis] Bump macOS CMake target image (Fuzzbawls)
 - #1710 `ed63a331a3` [Depends] Update dependency fallback URL (Fuzzbawls)

### P2P Protocol and Network Code
 - #1542 `87feface84` [Net] Add and document network messages in protocol.h (Fuzzbawls)
 - #1579 `e941230894` [Net] Use SeedSpec6's rather than CAddress's for fixed seeds (Cory Fields)
 - #1616 `f295ee4e4e` [P2P] Enforce expected outbound services (Pieter Wuille)
 - #1618 `077ab3db70` [P2P] Do not set extra flags for unfiltered DNS seed results (Pieter Wuille)
 - #1638 `cadf9e6daa` [Net] Fix masking of irrelevant bits in address groups (Alex Morcos)
 - #1646 `4c829bb274` [Net] Turn net structures into dumb storage classes (furszy)
 - #1672 `745a05804f` [Bug] removed "dseg" message support re-introduced. (furszy)
 - #1704 `8bbc0650e6` [Net] Pre-requirements for network encapsulation (random-zebra)

### GUI
 - #1059 `39a0fa6e04` [UI] Improve staking chart workflow (Akshay)
 - #1287 `e7dd0947c0` [GUI] Load persisted transaction filter during start (Mrs-X)
 - #1516 `93df7ce6ec` [GUI] MacOS fix open files with no default app. (furszy)
 - #1548 `176d3ae558` [Cleanup][GUI] Remove zHMS faqs (random-zebra)
 - #1549 `47bf23aa14` [Cleanup][GUI] Nuke zHMS from the GUI (random-zebra)
 - #1598 `f66f72656d` [GUI] Split "Delegators" address type in the table model (furszy)
 - #1601 `78a2923184` [GUI] Tor state missing translation (furszy)
 - #1604 `8ec6bbe737` [Refactor][GUI] Set static texts in .ui files + add missing tr() (random-zebra)
 - #1607 `749d42a812` [QT] Add password mismatch warnings (JSKitty)
 - #1610 `9ac68cffd5` [GUI][UX] Add accept/close keyboard controls to all dialogs (random-zebra)
 - #1611 `3d9222edf0` [GUI][Bug] CoinControl payAmounts and nBytes calculation (random-zebra)
 - #1612 `eea3f42ab6` [GUI] CoinControlDialog fix SelectAll / UnselectAll (random-zebra)
 - #1623 `b7e2d97a42` [GUI][Trivial] Change Custom fee amount when Recommended fee is selected (Ambassador)
 - #1626 `5b064a6145` [GUI][Bug] Notify transaction creation failure reason (random-zebra)
 - #1644 `34bf1c0583` [GUI] Fix invalid Destination toString encoding. (Fuzzbawls)
 - #1652 `a2850ce0fe` [GUI] cs widget: minor usability improvements (random-zebra)
 - #1653 `6ad9bfcc48` [GUI] Reuse collateral UTXOs in MnWizard (random-zebra)
 - #1657 `169fc34192` [GUI][Model] Remove unneeded lock in checkBalanceChanged (furszy)
 - #1667 `730566d510` [GUI] removing unused bitcoinamountfield file (furszy)
 - #1678 `fb14a8fb46` [GUI][Model] Remove zerocoin methods. (furszy)
 - #1683 `68e1ee69e5` [GUI] Add missing QPainterPath include (Fuzzbawls)
 - #1688 `129563c273` [GUI] TopBar navigation (sync/peers) (random-zebra)
 - #1690 `12a8f8a5c4` [BUG][GUI] check for available "unlocked" balance in prepareTransaction (random-zebra)
 - #1696 `f84445af1e` [BUG][GUI] Restore message() connection for ColdStakingWidget (random-zebra)
 - #1699 `9bba19c62e` [GUI] Fix dashboard txs list blinking issue (furszy)
 - #1708 `f3b54df58d` [BUG][GUI] Non-static coinControl object in CoinControlDialogs (random-zebra)
 - #1715 `37c01cf359` [GUI][Trivial] Update settingsfaqwidget.ui (Jeffrey)
 - #1724 `c178aab2fe` [GUI] Return early in pollBalanceChanged (Fuzzbawls)
 - #1732 `be99df3342` [GUI] Better start single GM error description. (furszy)
 - #1737 `78ed084999` [GUI] Replace more deprecated Qt methods (Fuzzbawls)
 - #1740 `141141c9cf` [GUI] Periodic make translate (Fuzzbawls)
 - #1741 `ffae965fad` [GUI] Settings console message moved to read only. (furszy)
 - #1742 `a94f8f9a90` [GUI] Fix settings language combobox initialization. (furszy)
 - #1744 `fda84ee710` [GUI] Recognize key event for clearing console (Fuzzbawls)
 - #1749 `e182e112bc` [GUI] Solving old, not loaded at startup, transactions notification issue. (furszy)

### RPC/REST
 - #1640 `0b84a5025d` [P2P][RPC] Rework addnode behaviour (Pieter Wuille)
 - #1660 `10876c6c80` [RPC] Change btc to HMS in help text (PeterL73)
 - #1663 `0724bbbad2` [Wallet][RPC] FundTransaction - fundrawtransaction (random-zebra)
 - #1702 `7a849ca06a` [RPC] Table registration update and wallet table decoupled. (furszy)
 - #1731 `fe845a83d2` [RPC][Wallet] Deprecate internal account system (Fuzzbawls)

### Wallet
 - #1586 `73c3ecc388` [Wallet] Minimum value for stake split threshold (random-zebra)
 - #1695 `98cf582557` [Wallet] Abandon tx in CommitTransaction if ATMP fails (random-zebra)
 - #1698 `ccecffd144` [Wallet] Gamemasters start performance boost (furszy)
 - #1713 `f31b51f08e` [Refactor] zerocoin code moved from wallet.cpp to wallet_zerocoin.cpp (furszy)
 - #1717 `f928674bf7` [Wallet] AddToWallet split in AddToWallet and LoadToWallet (furszy)
 - #1734 `78a4f68ab2` [Wallet][BUG] fix detection of wallet conflicts in CWallet::AddToWallet (random-zebra)

### Scripts and Tools
 - #1693 `a5265a4db4` Upstream scripts back ports [Step 1] (furszy)
 - #1701 `f8f0fe0bf1` [Tools] logprint-scanner for error() and strprintf() (random-zebra)

### Miscellaneous
 - #1547 `1824def118` [Refactor] Define constant string variable for currency unit (random-zebra)
 - #1568 `8c06746f18` [Trivial] Unused Image Removal & Readme Revisions (Yuurin Bee)
 - #1643 `80c5499b39` Fix Destination wrapper segfault on copy constructor (furszy)
 - #1659 `494c841f7e` Few compiler warnings fixed. (furszy)
 - #1662 `5d3266c25c` [Cleanup] Remove obsolete multisig code (random-zebra)
 - #1674 `360ee1dfc5` Remove unused MultiSigScriptSet typedef and member (furszy)
 - #1689 `9205aa6277` [Cleanup] Small main.cpp cleanup (furszy)
 - #1692 `1221efd5d6` [Cleanup] Don't sign MESS_VER_STRMESS network msgs anymore (random-zebra)
 - #1705 `2d6835e734` [Doc] Add/Update some release notes for 4.2 (random-zebra)
 - #1718 `5e75d4c344` [Trivial] Spelling Error (Yuurin Bee)
 - #1719 `b72ec2ff52` [Cleanup] remove gamemaster broadcast from CActiveGamemaster class (random-zebra)
 - #1722 `f68acbe01e` Unused methods cleanup (furszy)
 - #1736 `fc5d36ac77` [Cleanup] Transaction primitive unused methods cleanup. (furszy)
 - #1751 `dbcbf8a995` [Miner] Unused reserveKey cleanup (furszy)

## Credits

Thanks to everyone who directly contributed to this release:
- Akshay
- Alex Morcos
- Ambassador
- Ben Woosley
- Cory Fields
- Ethan Heilman
- Fuzzbawls
- Gavin Andresen
- Gregory Maxwell
- JSKitty
- Jeffrey
- Jeremy Rubin
- Jiaxing Wang
- John Newbery
- Jonas Schnelli
- João Barbosa
- Kaz Wesley
- Marko Bencun
- Patrick Strateman
- Peter Todd
- PeterL73
- Pieter Wuille
- Suhas Daftuar
- Wladimir J. van der Laan
- Yuurin Bee
- furszy
- practicalswift
- random-zebra


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.