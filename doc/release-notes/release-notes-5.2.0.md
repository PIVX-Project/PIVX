hemis Core version v5.2.0 is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including various bug fixes and visual improvements.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>

Mandatory Update
==============

hemis Core v5.2.0 is a mandatory update for all users. This release contains a protocol upgrade (to `70922`), new consensus rules and improvements that are not backwards compatible with older versions. Users will need to update their clients before enforcement of this update goes into effect.

Update enforcement is currently scheduled to go into effect at the following block height:
```
Mainnet: 2,927,000
```
Masternodes will need to be restarted once both the masternode daemon and the controller wallet have been upgraded.

Note: In preparation for the enforcement, upgraded peers will start rejecting non-upgraded peers few hours before the enforcement block height, we recommend everyone to be updated at max a day before the final time.

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).

Sapling Parameters
==================

In order to run, hemis Core now requires two files, `sapling-output.params` and `sapling-spend.params` (with total size ~50 MB), to be saved in a specific location.

For the following packages, no action is required by the user:
- macOS release `dmg` binaries will use the params that are bundled into the .app bundle.
- Windows installer `.exe` will automatically copy the files in the proper location.
- Linux `PPA/Snap` installs will automatically copy the files in the proper location.

For the other packages, the user must save the param files in the proper location, before being able to run hemis v5.2.0:
- macOS/Linux `tar.gz` tarballs include a bash script (`install-params.sh`) to copy the parameters in the appropriate location.
- Windows `.zip` users need to manually copy the files from the `share/hemis` folder to the `%APPDATA%\hemisParams` directory.
- self compilers can run the script from the repository sources (`params/install-params.sh`), or copy the files directly from the `params` subdirectory.

Compatibility
==============

hemis Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.12+, and Windows 7 and later.

hemis Core should also work on most other Unix-like systems but is not frequently tested on them.

v5.2.0 Change log
==============

### GUI
- #2259 fix QT 5.15 `currentIndexChanged(QString)` obsolete method call (furszy)
- #2260 Generate FAQ answer content programmatically (Fuzzbawls)
- #2305 Show the window when double clicking the taskbar icon (furszy)
- #2353 [GUI] Add search option to My Addresses list in receive widget (PicklesRcool)
- #2374 [Backport] Fix memory leaks in qt/guiutil.cpp (random-zebra)
- #2379 [BugFix] fix "not updating GUI balance" race condition (furszy)
- #2377 [BugFix][GUI] Fix minimize and close bugs (furszy)
- #2401 [GUI] BugFix, disappearing text in comboboxes (furszy)

### Consensus
- #2258 [Script] Fix "split P2CS" vulnerability (random-zebra)
- #2428 [Consensus] Introduce 5.2 nuparams and use them to guard new P2CS rules (random-zebra)
- #2431 [Consensus] Bump active protocol version to 70922 (random-zebra)
- #2435 [Backport] Bump default block version to 10 (no enforcement) (random-zebra)
- #2441 [Consensus] v5.2 upgrade enforcement height (furszy).

### Tier Two Network (Masternodes / Budget)
- #2395 [budget] partially fixing a race condition that could cause a good peer to be banned (furszy)
- #2437 [Backport] Add tiertwo_governance_reorg functional test (random-zebra)
- ##2440 Align budget payee validity to highest voted budget check. (furszy)

### Build system:
- #2350 [build] depends macOS: point --sysroot to SDK (furszy)
- #2384 [depends] update Qt 5.9 source url (furszy)
- #2413 [CI] Remove Ubuntu 16.04 usage from GA workflow (furszy)
- #2433 [Build] Bump gitian descriptor version to 5.2 (fuzzbawls)

### Wallet
- #2450 [wallet] fix segfault for P2CS inputs in CreateTransaction (furszy)

### Miscellaneous
- #2348 [Cleanup] Cleaning unused gitian fingerprints (furszy)

## Credits

Thanks to everyone who directly contributed to this release:

- Dan Raviv
- furszy
- Fuzzbawls
- ken2812221
- Kittywhiskers Van Gogh
- random-zebra
- PicklesRcool
- Sjors Provoost
- Volodia


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.