hemis Core version v5.4.0 is now available from: <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including a brand-new visual graphical interface for the Governance system, tier two network stability improvements, various bug fixes and performance enhancements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down, then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).

Notable Changes
==============

## GUI:

* Brand-new governance graphic user interface! Styled for both themes, light and dark! Now you will be able to:
  - Watch and follow active proposals live.
  - Create new proposal using a simple three steps wizard (The wallet will automatically create the transaction and relay the proposal to the network without any further interaction).
  - Follow how many blocks left for the next superblock, the allocated and the available amounts.
  - Vote for proposals functionality with a multi-gamemaster voting wizard and change vote on-demand.
  - Filter proposals by status, votes count and more.
* Gamemaster rewards have been included into the dashboard chart.
* Address search functionality for cold-staking screen.
* More precise after-fee calculation.
* Wallet startup screen presenting needed disk space per-network.
* Fix "null" name for wallet automatic backups.
* Fix bug during restart process that blocked the wallet for being automatically restarted.

## Wallet

* Improved transaction inputs selection, preferring coins that have fewer ancestors (preventing tx rejection for long mempool chains).

* Much faster shield transactions processing during block reception (faster synchronization for large wallets).

## RPC Server

### Deprecated 'autocombinerewards' Command

The `autocombinerewards` RPC command was soft-deprecated in v5.3.0 and replaced with explicit setter/getter commands `setautocombinethreshold`/`getautocombinethreshold`. hemis Core, by default, will no longer accept the `autocombinerewards` command, returning a deprecation error, unless the `hemisd`/`hemis-qt` is started with the `-deprecatedrpc=autocombinerewards` option.

This command will be fully removed in v6.0.0.

### Shield address support for RPC label commands

The `setlabel` RPC command now supports a shield address input argument to allow users to set labels for shield addresses. Additionally, the `getaddressesbylabel` RPC command will also now return shield addresses with a matching label.

### Specify optional label for getnewshieldaddress

The `getnewshieldaddress` RPC command now takes an optional argument `label (string)` to denote the desired label for the generated address.

P2P connection management
--------------------------

- Peers manually added through the addnode option or addnode RPC now have their own
  limit of sixteen connections which does not compete with other inbound or outbound
  connection usage and is not subject to the maxconnections limitation.

- New connections to manually added peers are much faster.

v5.4.0 Change log
=================

### Block and transaction handling
- #2549 Remove temporary guard for 5.3 rules pre-enforcement (random-zebra)
- #2552 Restore pre-v5.3 guard for under-minting blocks rule (random-zebra)
- #2591 Clean zc transactions from check() method (furszy)

### P2P protocol and network code
- #2587 Split resolve out of connect and add addnode parallel outbound connections limit (furszy)
- #2677 [net_processing] Fix ignoring get data requests when fPauseSend is set on a peer (furszy)
- #2682 [net] Remove assert(nMaxInbound > 0) (furszy)

### Wallet
- #2539 Sapling, restructure increment witnesses workflow (furszy)
- #2625 Fix "null" backup file name (furszy)
- #2601 Prefer coins that have fewer ancestors, sanity check txn before ATMP and rebroadcast functional test (furszy)

### Tier two network
- #2565 [BUG] Consider also DGMs in CountEnabled and CountNetworks (random-zebra)
- #2567 [BUG][TierTwo] GMB process refactor (furszy)
- #2659 [TierTwo] Fix and improve several synchronization problems (furszy)

### GUI
- #2534 Add address search option in cold-staking widget (random-zebra)
- #2593 [BUG] set locked label invisible for shield notes (furszy)
- #2568 [BUG][GUI] Refine bytes/fee/"after fee" calculation in coin-control (random-zebra)
- #2406 New Governance Graphical User Interface (furszy)
- #2626 [BugFix] Schedule proposal broadcast only if the block has not passed yet (furszy)
- #2627 Correct proposal large title for being cut when the app window width isn't big enough (furszy)
- #2621 Show disk space requirement per-network (Fuzzbawls)
- #2635 [Cleanup] Stop translating placeholder strings (Fuzzbawls)
- #2577 Include GM Rewards in the dashboard chart (furszy)
- #2564 Clean restart process (furszy)
- #2661 [BUG] GUI: invalid locking of shield notes in coin control (random-zebra)
- #2662 [GUI] Don't show UTXO locking options for shield notes (Fuzzbawls)
- #2663 [Build] Sanitize governance UIs item naming (Fuzzbawls)
- #2671 [GUI] Differentiate Budget payment output records from GMs block reward records (furszy)
- #2664 [GUI] Fix proposal tooltip menu location skewing (Fuzzbawls)
- #2670 [GUI] Governance, do not open the proposal creation wizard if node is in IBD (furszy)
- #2665 [GUI] Fix governance nav button hover css (Fuzzbawls)
- #2672 [GUI] Fix proposal name and URL size limit validation (furszy)

### Build system
- #2521 Add M1 mac homebrew path (Fuzzbawls)
- #2522 Use latest config guess/sub (Fuzzbawls)
- #2581 [Cleanup] Remove un-used TravisCI related files (Fuzzbawls)
- #2523 Bump gmp to v6.2.1 (Fuzzbawls)
- #2582 Fix Qt builds for M1 apple builds (Fuzzbawls)
- #2588 Remove unused cargo-checksum.sh script (Fuzzbawls)
- #2583 Bump Rust to v1.54.0 (Fuzzbawls)
- #2589 [CMake] Further Apple M1 path improvements (Fuzzbawls)

### RPC and other APIs
- #2540 [Doc] Fix RPC/cli example in setautocombinethreshold help (random-zebra)
- #2600 getnewshieldaddress add 'label' argument (furszy)
- #2610 Remove auth cookie on shutdown (furszy)
- #2608 Fully deprecate the autocombinerewards method (Fuzzbawls)
- #2609 Shield address setlabel/getaddressesbylabel (Fuzzbawls)

### Tests and QA
- #2494 [Test] Script tests updates (furszy)
- #2420 [BLS] Add wrapper around chiabls lib, worker, benchmarks and unit tests (random-zebra)
- #2550 Add GM payments test coverage (furszy)
- #2538 Improve benchmark precision (furszy)
- #2616 Replace usage of tostring() with tobytes() (Fuzzbawls)
- #2585 Secondary chains acceptance test coverage (furszy)
- #2590 ci: Enable shellcheck linting (Fuzzbawls)
- #2633 sapling_wallet_nullifiers.py whitelist peers to speed up tx relay (furszy)
- #2634 fix coinstake input selection on secondary chains (furszy)
- #2638 Fix intermittent mempool sync failures in sapling_wallet_nullifiers (random-zebra)
- #2619 Add test case for node with a full "ask for" inv set (furszy)
- #2641 Add linter for circular dependencies and start fixing them (random-zebra)

### Miscellaneous
- #2493 Locked memory manager updates (furszy)
- #2547 util: Replace logprintf (but not logprint) macro with regular function (random-zebra)
- #2545 util: Better url validation (random-zebra)
- #2548 util: Add validation interface logging (random-zebra)
- #2558 Cleaning unused code (furszy)
- #2629 util: Support serialization of std::vector<bool> (random-zebra)
- #2586 [Cleanup] Remove fNetworkNode and pnodeLocalHost, and encapsulate CNode id. (furszy)
- #2579 hemis-cli better error handling + libevent RAII upstream backports (furszy)

### Refactoring and cleanups
- #2569 [Cleanup] Remove and deglobalize system.h global variables (furszy)
- #2603 [Cleanup] Remove unused chain params (furszy)
- #2630 [Cleanup] Remove unused variables and private fields (random-zebra)
- #2642 scripted-diff: replace boost::optional with Optional<> wrapper (random-zebra)
- #2646 [Refactoring] Break circ dependency init -> * -> init by extracting shutdown.h
- #2643 [Refactoring] Remove checkpoints circular dependencies (random-zebra)
- #2644 [Refactoring] remove circular dependency primitives/transaction <-> script/standard (random-zebra)

## Credits

Thanks to everyone who directly contributed to this release:

- Alexander Block
- Anthony Towns
- Carl Dong
- Chris Stewart
- Cory Fields
- furszy
- Fuzzbawls
- gmaxwell
- Gregory Maxwell
- Gregory Sanders
- Gregory Solarte
- instagibbs
- Jeffrey Czyz
- Jim Posen
- John Newbery
- Kalle Alm
- Karl-Johan Alm
- Kaz Wesley
- Luke Dashjr
- MarcoFalke
- Martin Ankerl
- Matt Corallo
- Nikolay Mitev
- Pavel Janík
- Pieter Wuille
- practicalswift
- Puru
- random-zebra
- Russell Yanofsky
- Suhas Daftuar
- Wladimir J. van der Laan

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), and every single person who tested the release candidates.
