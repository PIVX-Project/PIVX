hemis Core version *3.2.0* is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>


Mandatory Update
==============

hemis Core v3.2.0 is a **mandatory update** for all block creators, gamemasters, and integrated services (exchanges). Old version 4 blocks will be rejected once 95% of a rolling 7 days worth of blocks have signaled the new version 5.

Gamemasters will need to be restarted once both the gamemaster daemon and the controller wallet have been upgraded.

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).

Wallets for existing users upgrading from an earlier version will undergo a supply recalculation the first time v3.2.0 is started. An initial light weight (partial) recalculation will be attempted first, but if that fails then the wallet will do a full recalculation the next time it is started. This recalculation is a one-time event.

Compatibility
==============

hemis Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.10+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

Apple released it's last Mountain Lion update August 13, 2015, and officially ended support on [December 14, 2015](http://news.fnal.gov/2015/10/mac-os-x-mountain-lion-10-8-end-of-life-december-14/). hemis Core software starting with v3.2.0 will no longer run on MacOS versions prior to Yosemite (10.10). Please do not report issues about MacOS versions prior to Yosemite to the issue tracker.

hemis Core should also work on most other Unix-like systems but is not frequently tested on them.

 
Notable Changes
==============

Minimum Supported MacOS Version
------

The minimum supported version of MacOS (OSX) has been moved from 10.8 Mountain Lion to 10.10 Yosemite. Users still running a MacOS version prior to Yosemite will need to upgrade their OS if they wish to continue using the latest version(s) of the hemis Core wallet.

Attacks, Exploits, and Mitigations
------

### Fake Stake

On Janurary 22 2019, Decentralized Systems Lab out of the University of Illinois published a study entitled “[‘Fake Stake’ attacks on chain-based Proof-of-Stake cryptocurrencies](https://medium.com/@dsl_uiuc/fake-stake-attacks-on-chain-based-proof-of-stake-cryptocurrencies-b8b05723f806)”, which outlined a type of Denial of Service attack that could take place on a number of Proof of Stake based networks by exhausting a client's RAM or Disk resources.

A full report provided by hemis developers is available on the [hemis Website](https://hemis.org/fake-stake-official-hemis-report/), which includes additional findings, mitigation details, and resources for testing. This type of attack has no risk to users' privacy and does not affect their holdings.

### Wrapped Serials

On March 6th 2019, an attack was detected on the hemis network zerocoin protocol, or zHMS. The vulnerability allows an attacker to fake serials accepted by the network and thus to spend zerocoins that have never been minted. As severe as it is, it does not harm users’ privacy and does not affect their holdings directly.

As a result of this, all zHMS functionality was disabled via one of our sporks shortly after verification of this exploit. A full report, detailing how this attack was performed, as well as investigation results and mitigation methods is available [On Medium](https://medium.com/@dev.hemis/report-wrapped-serials-attack-5f4bf7b51701).

zHMS functions will be restored after v3.2.0 is pushed out and the majority of the network has upgraded.

Major New Features
------

### BIP65 (CHECKLOCKTIMEVERIFY) Soft-Fork

hemis Core v3.2.0 introduces new consensus rules for scripting pathways to support the [BIP65](https://github.com/bitcoin/bips/blob/master/bip-0065.mediawiki) standard. This is being carried out as a soft-fork in order to provide ample time for stakers to update their wallet version.

### Automint Addresses

A new "Automint Addresses" feature has been added to the wallet that allows for the creation of new addresses who's purpose is to automatically convert any HMS funds received by such addresses to zHMS. The feature as a whole can be enabled/disabled either at runtime using the `-enableautoconvertaddress` option, via RPC/Console with the `enableautomintaddress` command, or via the GUI's options dialog, with the default being enabled.

Creation of these automint addresses is currently only available via the RPC/Console `createautomintaddress` command, which takes no additional arguments. The command returns a new hemis address each time, but addresses created by this command can be re-used if desired.

### In-wallet Proposal Voting

A new UI wallet tab has been introduced that allows users to view the current budget proposals, their vote counts, and vote on proposals if the wallet is acting as a gamemaster controller. The visual design is to be considered temporary, and will be undergoing further design and display improvements in the future.

### Zerocoin Lite Node Protocol

Support for the ZLN Protocol has been added, which allows for a node to opt-in to providing extended network services for the protocol. By default, this functionality is disabled, but can be enabled by using the `-peerbloomfilterszc` runtime option.

A full technical writeup of the protocol can be found [Here](https://hemis.org/wp-content/uploads/2018/11/Zerocoin_Light_Node_Protocol.pdf).

### Precomputed Zerocoin Proofs

This introduces the ability to do most of the heavy computation required for zHMS spends **before** actually initiating the spend. A new thread, `ThreadPrecomputeSpends`, is added which constantly runs in the background.

`ThreadPrecomputeSpends`' purpose is to monitor the wallet's zHMS mints and perform partial witness accumulations up to `nHeight - 20` blocks from the chain's tip (to ensure that it only ever computes data that is at least 2 accumulator checkpoints deep), retaining the results in memory.

Additionally, a file based cache is introduced, `precomputes.dat`, which serves as a place to store any precomputed data between sessions, or when the in-memory cache size is exhausted. Swapping data between memory and disk file is done as needed, and periodic cache flushes to the disk are routine.

This also introduces 2 new runtime configuration options:

* `-precompute` is a binary boolean option (`1` or `0`) that determines wither or not pre-computation should be activated at runtime (default value is to activate, `1`).
* `-precomputecachelength` is a numeric value between `500` and `2000` that tells the precompute thread how many blocks to include during each pass (default is `1000`).

A new RPC command, `clearspendcache`, has been added that allows for the clearing/resetting of the precompute cache (both memory and disk). This command takes no additional arguments.

Finally, the "security level" option for spending zHMS has been completely removed, and all zHMS spends now spend at what was formerly "security level" `100`. This change has been reflected in any RPC command that previously took a security level argument, as well as in the GUI's Privacy section for spending zHMS.

### Regression Test Suite

The RegTest network mode has been re-worked to once again allow for the generation of on-demand PoW and PoS blocks. Additionally, many of the existing functional test scripts have been adapted for use with hemis, and we now have a solid testing base for highly customizable tests to be written.

With this, the old `setgenerate` RPC command no longer functions in regtest mode, instead a new `generate` command has been introduced that is more suited for use in regtest mode.

GUI Changes
------

### Console Security Warning

Due to an increase in social engineering attacks/scams that rely on users relaying information from console commands, a new warning message has been added to the Console window's initial welcome message.

### Optional Hiding of Orphan Stakes

The options dialog now contains a checkbox option to hide the display of orphan stakes from both the overview and transaction history sections. Further, a right-click context menu option has been introduced in the transaction history tab to achieve the same effect.

**Note:** This option only affects the visual display of orphan stakes, and will not prevent them nor remove them from the underlying wallet database.

### Transaction Type Recoloring

The color of various transaction types has been reworked to provide better visual feedback. Staking and gamemaster rewards are now purple, orphan stakes are now light gray, other rejected transactions are in red, and normal receive/send transactions are black.

### Receive Tab Changes

The address to be used when creating a new payment request is now automatically displayed in the form. This field is not user-editable, and will be updated as needed by the wallet.

A new button has been added below the payment request form, "Receiving Addresses", which allows for quicker access to all the known receiving addresses. This one-click button is the same as using the `File->Receiving Addresses...` menu command, and will open up the Receiving Addresses UI dialog.

Historical payment requests now also display the address used for the request in the history table. While this information was already available when clicking the "Show" button, it was an extra step that shouldn't have been necessary.

### Privacy Tab Changes

The entire right half of the privacy tab can now be toggled (shown/hidden) via a new UI button. This was done to reduce "clutter" for users that may not wish to see the detailed information regarding individual denomination counts.

RPC Changes
------

### Backupwallet Sanity

The `backupwallet` RPC command no longer allows for overwriting the currently in use wallet.dat file. This was done to avoid potential file corruption caused by multiple conflicting file access operations.

### Spendzerocoin Security Level Removed

The `securitylevel` argument has been removed from the `spendzerocoin` RPC command.

### Spendzerocoinmints Added

Introduce the `spendzerocoinmints` RPC call to enable spending specific zerocoins, provided as an array of hex strings (serial hashes).

### Getreceivedbyaddress Update

When calling `getreceivedbyaddress` with a non-wallet address, return a proper error code/message instead of just `0`

### Validateaddress More Verbosity

`validateaddress` now has the ability to return more (non-critical or identifying) details about P2SH (multisig) addresses by removing the needless check against ISMINE_NO.

### Listmintedzerocoins Additional Options

Add a `fVerbose` boolean optional argument (default=false) to `listmintedzerocoins` call to have a more detailed output.

If `fVerbose` is specified as first argument, then a second optional boolean argument `fMatureOnly` (default=false) can be used to filter-out immature mints.

### Getblock & Getblockheader

A minor change to these two RPC commands to now display the `mediantime`, used primarialy during functional tests.

### Getwalletinfo

The `getwalletinfo` RPC command now outputs the configured transaction fee (`paytxfee` field).

Build System Changes
------

### Completely Disallow Qt4

Compiling the hemis Core wallet against Qt4 hasn't been supported for quite some time now, but the build system still recognized Qt4 as a valid option if Qt5 couldn't be found. This has now been remedied and Qt4 will no longer be considered valid during the `configure` pre-compilation phase.

### Further OpenSSL Deprecation

Up until now, the zerocoin library relied exclusively on OpenSSL for it's bignum implementation. This has now been changed with the introduction of GMP as an arithmetic operator and the bignum implementation has now been redesigned around GMP. Users can still opt to use OpenSSL for bignum by passing `--with-zerocoin-bignum=openssl` to the `configure` script, however such configuration is now deprecated.

**Note:** This change introduces a new dependency on GMP (libgmp) by default.

### RISC-V Support

Support for the new RISC-V 64bit processors has been added, though still experimental. Pre-compiled binaries for this CPU architecture are available for linux, and users can self-compile using gitian, depends, or an appropriate host system natively.

### New Gitian Build Script

The previous `gitian-build.sh` shell script has been replaced with a more feature rich python version; `gitian-build.py`. This script now supports the use of a docker container in addition to LXC or KVM virtualization, as well as the ability to build against a pull request by number.

*3.2.0* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Build System
 - #758 `81c7c4764c` [Depends] Update libsecp256k1 to latest master (warrows)
 - #804 `4a8e46a158` [Depends] Update zmq to 4.3.1 (Dimitris Apostolou)
 - #795 `1920f3e8ad` [Build] Add  support for RISC-V and build it with gitian (cevap)
 - #786 `44226f225e` [Build] add gitian build python script (cevap)
 - #783 `ccba68e425` [Depends] Update QT to 5.9.7 (cevap)
 - #754 `b9cbeb0951` [Build] Update Build/Depends systems from upstream (Fuzzbawls)
 - #752 `63fb77b0a9` [Build] Fix Thread Safety Analysis Warnings (Fuzzbawls)
 - #749 `36ff23553c` [Build] Update genbuild.sh script (Fuzzbawls)
 - #681 `95ec0763cf` [Depends] Add gmp bignum support for zerocoin lib (warrows)
 - #704 `f0a427bfd7` [Build] GCC-7 and glibc-2.27 back compat (Fuzzbawls)
 - #706 `d3c5b808dd` [Build] Remove throw keywords in leveldb function signatures (Fuzzbawls)
 - #708 `72cd07186b` [Build] Remove stale m4 file (Fuzzbawls)
 - #671 `b003052103` [Build] Update to latest leveldb (Fuzzbawls)

### P2P Protocol and Network Code
 - #843 `817cec4ff4` [Net] Increment Protocol Version (Fuzzbawls)
 - #837 `d241b5ed77` [Zerocoin][UNIT TEST][RPC] Wrapped serials. (random-zebra)
 - #803 `065f94118d` [NET] Invalid blocks from forks stored on disk fix + blocks DoS spam filter. (furszy)
 - #802 `ed0dd2a20a` [Refactor] Remove begin/end_ptr functions (warrows)
 - #768 `204c038a4d` [Net] Zerocoin Light Node Protocol (furszy)
 - #798 `a663bccea7` [Net] Improve addrman Select() performance when buckets are nearly empty (Pieter Wuille)
 - #800 `7fa20d69f6` [Net] nLastTry is only used for addrman entries (Pieter Wuille)
 - #740 `5f7cb412a3` [Net] Pull uacomment in from upstream (Fuzzbawls)
 - #774 `167c7fa6d0` [Budget] Make checks for GM-autovoting deterministic (Mrs-X)
 - #770 `ab9cf3629c` [Main] Do not record zerocoin tx's in ConnectBlock() if it is fJustCheck (presstab)
 - #705 `6a5b64bc21` [Main] Check Lock Time Verify (presstab)

### GUI
 - #850 `e488db7334` [Qt] Update localizations from Transifex (Fuzzbawls)
 - #847 `fc924c1f63` [Qt] Fix to display missing clock5.png tx image (joeuhren)
 - #840 `757d81c4a9` [QT] cleanup, remove old trading dialog form (furszy)
 - #826 `0d738b3dc0` [Qt] Fix a windows only crash when r-clicking a proposal (warrows)
 - #792 `c12697469b` [UI] Add a budget monitoring and voting tab (warrows)
 - #794 `8dcb52fcd4` [UI] Open related options tab when clicking automint icon (warrows)
 - #791 `c0aa454e19` [Qt] Fix Missing Explorer Icon (sicXnull)
 - #779 `d617c85a89` [Qt] Periodic translation update (Fuzzbawls)
 - #781 `10e1a8a306` [Qt] Don't show staking/automint status icons without a wallet (Fuzzbawls)
 - #776 `3fcdc932d9` [Qt] Add a security warning to the debug console's default output. (Fuzzbawls)
 - #747 `feb87c10fa` [GUI] Hide orphans - contextMenu action (random-zebra)
 - #741 `ea2637838c` [GUI] Sort by 'data' in zHMS and coin control dialogs (random-zebra)
 - #733 `9a792d73e9` [GUI] Hide orphans (random-zebra)
 - #735 `44840c5069` [Qt] Stop using dummy strings in clientversion.cpp (Fuzzbawls)
 - #725 `793db15baf` [UI] Sort numbers correctly in zHMS and coin control dialogs (random-zebra)
 - #714 `bf2ad88066` [UI] Add address field in receive tab (warrows)
 - #683 `ec1180b52c` [Qt] receivecoinsdialog - address control + clean UI (random-zebra)
 - #677 `29fab5982f` [Qt] change colors for tx labels in history/overview (random-zebra)
 - #693 `022b58257c` [UI] Add address to the payment request history (warrows)
 - #698 `3f35bc81d8` [Qt] Remove Qt4 build support & code fallbacks (Wladimir J. van der Laan)
 - #655 `de0c4e0888` [Qt] Fix HMS balances on overview page (Fuzzbawls)
 - #680 `71ac5285e5` [Qt] Privacy dialog: hide/show denominations (random-zebra)
 - #675 `8a26ba0b07` [Qt] SwiftX - intuitiveness (random-zebra)
 - #668 `4a68c9ed43` [Qt] Clean up Multisend Dialog UI (Fuzzbawls)

### RPC/REST
 - #838 `5673c8373e` [RPC][Test] spendrawzerocoin + wrapped serials functional test (random-zebra)
 - #821 `86d6133735` [RPC] Fixup signrawtransaction on regtest (Fuzzbawls)
 - #751 `e820cf3816` [RPC] Show the configured/set txfee in getwalletinfo (Fuzzbawls)
 - #750 `25fe72d97d` [RPC] Add mediantime to getblock/getblockheader output (Fuzzbawls)
 - #760 `8b79a3944a` [RPC] Show BIP65 soft-fork progress in getblockchaininfo (Fuzzbawls)
 - #742 `297d67b43a` [RPC] Add gamemaster's pubkey to listgamemasters RPC (presstab)
 - #729 `f84ec3df8b` [RPC] Fix RPCTimerInterface (random-zebra)
 - #727 `08f6e1774b` [RPC] Add 'spendzerocoinmints' RPC call (random-zebra)
 - #726 `8f28b7ad23` [RPC] include mints metadata in 'listmintedzerocoins' output (random-zebra)
 - #724 `ee0717c2af` [RPC] Ensure that a numeric is being passed to AmmountFromValue (Fuzzbawls)
 - #723 `0774f5fc0d` [RPC] Error when calling getreceivedbyaddress with non-wallet address (Fuzzbawls)
 - #722 `3ce4fd7226` [RPC] Add more verbosity to validateaddress (Fuzzbawls)
 - #721 `cecda14082` [RPC] Fix movecmd's help description to include amount (Fuzzbawls)
 - #720 `056b4d5cb1` [RPC] Sanitize walletpassphrase timeout argument (Fuzzbawls)
 - #719 `463fd38325` [RPC] Fix verifychain (Fuzzbawls)
 - #711 `17d1f30131` [RPC] Don't allow backupwallet to overwrite the wallet-in-use (Fuzzbawls)
 - #688 `64071d142d` [Zerocoin]  RPC import/export zerocoins private key standardized + Cleanup in AccPoK and SoK to avoid redundant calculations. (furszy)

### Wallet
 - #842 `c6c84fe85f` [Wallet] [zHMS] Precomputed Zerocoin Proofs (Fuzzbawls)
 - #817 `37a06eaa93` [Wallet] Fix segfault with runtime -disablewallet (Fuzzbawls)
 - #763 `d4762f7e7a` [Wallet] Add automint address (Fuzzbawls)
 - #759 `19fd0877cd` [Wallet] Avoid failed zHMS spend because of changed seed (warrows)
 - #755 `65be6b611b` [Wallet] Fix zHMS spend when too much mints are selected (warrows)
 - #734 `5df105fed2` [Staking] Ensure nCredit is correctly initialized in CreateCoinStake (warrows)
 - #730 `394d48b2c9` [Wallet] fix bug with fWalletUnlockAnonymizeOnly flag setting (random-zebra)
 - #715 `30048cce62` [Refactor] Remove GetCoinAge (Fuzzbawls)
 - #700 `a2d717090f` [Wallet] Avoid autocombine getting stuck (warrows)
 - #656 `5272a4f684` [Wallet] Fix double locked coin when wallet and GM are on same machine (Tim Uy)
 - #653 `fdf4503b66` [Wallet] change COINBASE_MATURITY to Params().COINBASE_MATURITY() (Alko89)

### Test Suites
 - #822 `2b8daac4c0` [Tests] Integrate fake stake tests into parent test suite (Fuzzbawls)
 - #812 `f8eb7feefc` [Regtest][Tests][RPC] Regtest mode + Test suite. (random-zebra)

### Miscellaneous
 - #788 `55ce1619f5` [Misc] Update license year 2019 (Everton Melo)
 - #736 `d2ad4d6e93` [Utils] Update linters for python3 (Fuzzbawls)
 - #699 `8b1f68d896` [Refactor] Use references instead of copies in for loops (Fuzzbawls)
 - #697 `5a5797f5c3` [Trivial] Remove Redundant Declarations (Fuzzbawls)
 - #667 `49f9a0fa9e` [Zerocoin] Clean zerocoin bignum file (warrows)
 - #669 `dd6909fd30` [Utils] Fix syntax error in gitian-build.sh (Aitor González)
 - #632 `0d91550ff6` [MoveOnly] Move non-wallet RPC files to subdir (Fuzzbawls)
 - #731 `f7f49cfa7c` [zHMS] Fix bignum overloads when using OpenSSL (Fuzzbawls)
 - #692 `1fde9b2b7a` [Zerocoin] Remove explicit copy assignement operator from Accumulator (warrows)
 - #761 `88a93bd35a` [Refactoring] Abstract out and switch openssl cleanse (Adam Langley)
 - #743 `af0c340fe0` [Refactor] remove CPubKey::GetHex (random-zebra)
 - #737 `434abd1ae9` [Refactor] Remove 'boost::lexical_cast<>' (random-zebra)
 - #769 `6482454cf6` [Main] Unify shutdown proceedure in init rather than per-app (Fuzzbawls)
 - #815 `decee4bc8c` [Doc] Update release notes with forthcoming 3.2.0 changes (Fuzzbawls)
 - #816 `51e7b2c4b0` [Doc] Update build-unix.md (Fuzzbawls)
 - #757 `a611a7fa77` [Doc] Update doc/build-windows.md (idas4you)
 - #778 `65caa886ac` [Doc] Update README.md (veilgets)
 - #703 `51663473fc` [Docs] Add missing automake dependency (Mrs-X)
 - #762 `abfceb39a1` [Random] WIN32 Seed Cleanup: Move nLastPerfmon behind win32 ifdef. (21E14)
 - #771 `4b1be14505` [Main] Clean up sync.cpp/h with upstream declarations (Fuzzbawls)
 
## Credits

Thanks to everyone who directly contributed to this release:

- 21E14
- Adam Langley
- Aitor González
- Alko89
- Benjamin Allred
- Chun Kuan Lee
- Cory Fields
- Dimitris Apostolou
- Everton Melo
- Fuzzbawls
- Matias Furszyfer
- Mrs-X
- Pieter Wuille
- SHTDJ
- Tim Uy
- Wladimir J. van der Laan
- blondfrogs
- cevap
- fanquake
- gpdionisio
- idas4you
- joeuhren
- presstab
- random-zebra
- s3v3nh4cks
- sicXnull
- veilgets
- warrows

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.
