(note: this is a temporary file, to be added-to by anybody, and moved to release-notes at release time)

PIVX Core version *version* is now available from:  <https://github.com/pivx-project/pivx/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/pivx-project/pivx/issues>


Mandatory Update
==============


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

(Developers: add your notes here as part of your pull requests whenever possible)

### Memory pool limiting

Previous versions of PIVX Core had their mempool limited by checking a transaction's fees against the node's minimum relay fee. There was no upper bound on the size of the mempool and attackers could send a large number of transactions paying just slighly more than the default minimum relay fee to crash nodes with relatively low RAM.

PIVX Core 4.2.0 will have a strict maximum size on the mempool. The default value is 300 MB and can be configured with the `-maxmempool` parameter. Whenever a transaction would cause the mempool to exceed its maximum size, the transaction that (along with in-mempool descendants) has the lowest total feerate (as a package) will be evicted and the node's effective minimum relay feerate will be increased to match this feerate plus the initial minimum relay feerate. The initial minimum relay feerate is set to 1000 satoshis per kB.

PIVX Core 4.2.0 also introduces new default policy limits on the length and size of unconfirmed transaction chains that are allowed in the mempool (generally limiting the length of unconfirmed chains to 25 transactions, with a total size of 101 KB). These limits can be overridden using command line arguments ([#1645](https://github.com/PIVX-Project/PIVX/pull/1645), [#1647](https://github.com/PIVX-Project/PIVX/pull/1647)).

### Benchmarking Framework

PIVX Core 4.2.0 backports  the internal benchmarking framework from Bitcoin Core, which can be used to benchmark cryptographic algorithms (e.g. SHA1, SHA256, SHA512, RIPEMD160, Poly1305, ChaCha20), Base58 encoding and decoding and thread queue. More tests are needed for script validation, coin selection and coins database, cuckoo cache, p2p throughtput ([#1650](https://github.com/PIVX-Project/PIVX/pull/1650)).

The binary file is compiled with pivx-core, unless configured with `--disable-bench`.<br>
After compiling pivx-core, the benchmarks can be run with:
```
src/bench/bench_pivx
```
The output will be similar to:
```
#Benchmark,count,min(ns),max(ns),average(ns),min_cycles,max_cycles,average_cycles
Base58CheckEncode,131072,7697,8065,7785,20015,20971,20242
```


GUI Changes
----------

### Topbar navigation

- The "sync" button in the GUI topbar can be clicked to go directly to the Settings --> Information panel (where the current block number and hash is shown).

- The "connections" button in the GUI topbar can be clicked to open the network monitor dialog ([#1688](https://github.com/PIVX-Project/PIVX/pull/1688)).

### Removed zerocoin GUI

Spending zPIV and getting zPIV balance information is no longer available in the graphical interface ([#1549](https://github.com/PIVX-Project/PIVX/pull/1549)). The feature remains accessible through the RPC interface: `getzerocoinbalance`, `listmintedzerocoins`, `listzerocoinamounts`, `spendzerocoin`, `spendzerocoinmints`.


Functional Changes
----------

### Stake-Split threshold

If the stake split is active (threshold > 0), then stake split threshold value must be greater than a minimum, set by default at 100 PIV. The minimum value can be changed using the `-minstakesplit` startup flag ([#1586](https://github.com/PIVX-Project/PIVX/pull/1586)). A value `0` is still allowed, regardless of the minimum set, and, as before, can be used to disable the stake splitting functionality.

### Changed command-line options

- new command `-minstakesplit` to modify the minimum allowed for  the stake split threshold ([#1586](https://github.com/PIVX-Project/PIVX/pull/1586)).

- new commands `-maxmempool`, to customize  the memory pool size limit, and `-checkmempool=N`, to customize the frequency of the mempool check ([#1647](https://github.com/PIVX-Project/PIVX/pull/1647)).

- new commands `-limitancestorcount=N` and `limitancestorsize=N`, to limit the number and total size of all in-mempool ancestors for a transaction ([#1647](https://github.com/PIVX-Project/PIVX/pull/1647)).

- new commands `-limitdescendantcount=N` and `limitdescendantsize=N`, to limit the number and total size of all in-mempool descendants for a transaction ([#1647](https://github.com/PIVX-Project/PIVX/pull/1647)).

Dependencies
------------

...


RPC Changes
------------

### Low-level API changes

- The `asm` property of each scriptSig now contains the decoded signature hash type for each signature that provides a valid defined hash type ([#1633](https://github.com/PIVX-Project/PIVX/pull/1633)).<br>
The following items contain assembly representations of scriptSig signatures
and are affected by this change: RPC `getrawtransaction`, RPC `decoderawtransaction`, REST `/rest/tx/` (JSON format), REST `/rest/block/` (JSON format when including extended tx details), `pivx-tx -json`

### Modified input/output for existing commands

- new "usage" field in the output of `getmempoolinfo`, displaying the total memory usage for the mempool ([#1645](https://github.com/PIVX-Project/PIVX/pull/1645)).

- new "upgrades" field in the output of `getblockchaininfo`, showing upcoming and active network upgrades ([#1665](https://github.com/PIVX-Project/PIVX/pull/1665), [#1687](https://github.com/PIVX-Project/PIVX/pull/1687)).

### Removed commands

### Newly introduced commands


*version* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core Features

### Build System

### P2P Protocol and Network Code

### GUI

### RPC/REST

### Wallet

### Miscellaneous

## Credits

Thanks to everyone who directly contributed to this release:


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/pivx-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.
