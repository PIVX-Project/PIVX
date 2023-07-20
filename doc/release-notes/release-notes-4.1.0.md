hemis Core version *4.1.0* is now available from:  <https://github.com/hemis-project/hemis/releases>

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

Hierarchical Deterministic Wallet (HD Wallet)
----------

Wallets under a tree derivation structure in which keypairs are generated deterministically from a single seed, which can be shared partially or entirely with different systems, each with or without the ability to spend coins, [BIP32](https://github.com/bitcoin/bips/blob/master/bip-0032.mediawiki).

Enabling major improvements over the keystore management, the hemis wallet doesn't require regular backups as before, keys are following a deterministic creation path that can be verified at any time (before HD Wallet, every keypair was randomly created and added to the keypool, forcing the user to backup the wallet every certain amount of time or could end up loosing coins forever if the latest `wallet.dat` was not being used).
As well as new possibilities like the account extended public key that enables deterministic public keys creation without the private keys requisite inside the wallet (A good use case could be online stores generating fresh addresses).

This work includes a customization/extension to the [BIP44](https://github.com/bitcoin/bips/blob/master/bip-0044.mediawiki) standard. We have included an unique staking keys derivation path which introduced the deterministic generation/recovery of staking addresses.

An extended description of this large work can be found in the PR [here](https://github.com/hemis-Project/hemis/pull/1327).

### HD Wallet FAQ

 - How do i upgrade to HD Wallet?

    GUI:
    1) A dialog will appear on every wallet startup notifying you that you are running a pre-HD wallet and letting you upgrade it from there.
    2) If you haven't upgraded your wallet, the topbar (bar with icons that appears at the top of your wallet) will have an "HD" icon. Click it and the upgrade dialog will be launched.

    RPC:
    1) If your wallet is unlocked, use the `-upgradewallet` flag at startup and will automatically upgrade your wallet.
    2) If your wallet is encrypted, use the `upgradewallet` rpc command. It will upgrade your wallet to the latest wallet version.

 - How do i know if i'm already running an HD Wallet?

    1) GUI: Go to settings, press on the Debug option, then Information.
    2) RPC: call `getwalletinfo`, the `walletversion` field must be `169900` (HD Wallet Feature).

Boosted wallet performance
----------

This release introduces a number of upgrades and improvements that greatly increase the wallet performance. RAM usage is reduced by ~64% and wallet's initial sync is ~42% faster.

Drop Windows 32-bit Binaries
----------

We are no longer shipping pre-compiled 32-bit Windows binaries as the demand for such binaries has been observed to be non-existent. While it may still be possible to self-compile for 32-bit Windows (x86), no efforts henceforth will be made to maintain that compatibility.

Any users currently running a 32-bit Windows OS should seek to upgrade to a 64-bit Windows OS in order to continue using the Core wallet now and in the future.

Removal of Partially translated locales
----------

From version 4.1.0 and onward, releases will no longer include any translation for languages that are not at least 80% translated.

MultiSend Disabled
----------

The MultiSend wallet feature has been effectively disabled as it's flow is clunky, out of date, and in need of a full re-code and review.

This has no effect on user funds.

GUI Changes
----------

### Keyboard navigation

Dialogs can now be accepted with the `ENTER` (`RETURN`) key, and dismissed with the `ESC` key ([#1392](https://github.com/hemis-Project/hemis/pull/1392)).

### Address sorting

Address sorting in "My Addresses" / "Contacts" / "Cold Staking" can now be customized, setting it either by label (default), by address, or by creation date, ascending (default) or descending order.
Addresses in the dropdown of the "Send Transaction" and "Send Delegation" widgets are now automatically sorted by label with ascending order ([#1393](https://github.com/hemis-Project/hemis/pull/1393)).

### Custom Fee

The custom fee selected when sending a transaction is now saved in the wallet database and persisted across multiple sends and wallet's restarts ([#1406](https://github.com/hemis-Project/hemis/pull/1406)). The fee is now also validated against the maximum value (10000 times `minRelayTxFee`) and minimum value (`minTxFee`) ([#1576](https://github.com/hemis-Project/hemis/pull/1576)).

### Include delegations in send

The send and cold-staking page present a checkbox to make the automatic input selection algorithm include delegated (P2CS) utxos if needed. The option is unchecked by default. ([#1391](https://github.com/hemis-Project/hemis/pull/1391))

### Optional Hiding of Staking Charts

The staking charts can now be hidden at startup (with a flag `--hidecharts`) or at runtime with a checkbox in settings --> options --> display ([#1475](https://github.com/hemis-Project/hemis/pull/1475)).

### Context Lock/Unlock

Present the unlock dialog directly (instead of an error message), whenever an action on encrypted/locked wallet requires full unlock.<br>

Restore the previous locking state ("locked" or "locked for staking only") when the action is completed. ([#1387](https://github.com/hemis-Project/hemis/pull/1387))

### External Change Address Warning

A new warning/confirmation dialog is displayed if a custom change address is not part of the wallet. ([#1527](https://github.com/hemis-Project/hemis/pull/1527))

### Cold Stake delegations marked in Coin Control

The Coin Control window now includes an icon next to the select checkbox when the UTXO is a Cold Stake delegation. This shares the space with the locked UTXO indicator icon, and locked UTXO's take priority in their icon display. ([#1470](https://github.com/hemis-Project/hemis/pull/1470))

### Hide zPIV balance info as needed

When the wallet contains no zPIV, the zPIV balance details will be hidden, reducing visual clutter.

### CSV Exporting

Transaction and address data can now be quickly exported to a CSV file from the Settings area of the GUI.

Transaction output format is comma separated with header row as follows:
```
"Confirmed","Date","Type","Label","Address","Amount (PIV)","ID"
```

Address output format is comma separated with header row as follows:
```
"Label","Address","Type"
```


Functional Changes
----------

### zPIV Backup Removed

Automatic zPIV backup has been disabled. Thus, the following configuration options have been removed  (either as entries in the hemis.conf file or as startup flags):
- `autozpivbackup`
- `backupzpiv`
- `zpivbackuppath`

### Stake-Split threshold

The stake split threshold is no longer required to be integer. It can be a fractional amount. A threshold value of 0 disables the stake-split functionality.

The default value for the stake-split threshold has been lowered from 2000 PIV, down  to 500 PIV.

### Changed command-line options

- `-debuglogfile=<file>` can be used to specify an alternative debug logging file. This can be an absolute path or a path relative to the data directory
- `-debugexclude=<category>` can be used to specify which debug categories to not log, useful when pairing with the `-debug=<exclude>` option.
- The `-reservebalance` configuration/startup option has been removed ([#1373](https://github.com/hemis-Project/hemis/pull/1373)).


Dependencies
------------

The minimum required version of QT has been increased from 5.0 to 5.5.1 (the [depends system](https://github.com/hemis-project/hemis/blob/master/depends/README.md) provides 5.9.7)


RPC Changes
------------

### Modified input/output for existing commands

- "CoinStake" JSON object in `getblock` output is removed, and replaced with the strings "stakeModifier" and "hashProofOfStake"
- "obfcompat" JSON field in `getmasternodecount` output is removed as it is/was redundant with the `enabled` field.
- "moneysupply" and "zpivSupply" attributes in `getblock` output are removed.
- "isPublicSpend" boolean (optional) input parameter is removed from the following commands:
  - `createrawzerocoinspend`
  - `spendzerocoin`
  - `spendzerocoinmints`
  - `spendrawzerocoin`

  These commands are now able to create only *public* spends (private spends were already enabled only on regtest).

- "mintchange" and "minimizechange" boolean input parameters are removed from the following commands:
  - `spendzerocoin`

  Mints are disabled, therefore it is no longer possible to mint the change of a zerocoin spend. The change is minimized by default.

- `setstakesplitthreshold` now accepts decimal amounts. If the provided value is `0`, split staking gets disabled. `getstakesplitthreshold` returns a double.

- `dumpwallet` no longer allows overwriting files. This is a security measure
   as well as prevents dangerous user mistakes.

- The output of `getstakingstatus` was reworked. It now shows the following information:
  ```
  {
     "staking_status": true|false,       (boolean) whether the wallet is staking or not
     "staking_enabled": true|false,      (boolean) whether staking is enabled/disabled in hemis.conf
     "coldstaking_enabled": true|false,  (boolean) whether cold-staking is enabled/disabled in hemis.conf
     "haveconnections": true|false,      (boolean) whether network connections are present
     "mnsync": true|false,               (boolean) whether masternode data is synced
     "walletunlocked": true|false,       (boolean) whether the wallet is unlocked
     "stakeablecoins": n,                (numeric) number of stakeable UTXOs
     "stakingbalance": d,                (numeric) PIV value of the stakeable coins (minus reserve balance, if any)
     "stakesplitthreshold": d,           (numeric) value of the current threshold for stake split
     "lastattempt_age": n,               (numeric) seconds since last stake attempt
     "lastattempt_depth": n,             (numeric) depth of the block on top of which the last stake attempt was made
     "lastattempt_hash": xxx,            (hex string) hash of the block on top of which the last stake attempt was made
     "lastattempt_coins": n,             (numeric) number of stakeable coins available during last stake attempt
     "lastattempt_tries": n,             (numeric) number of stakeable coins checked during last stake attempt
   }
   ```

### Removed commands

The following commands have been removed from the RPC interface:
- `createrawzerocoinstake`
- `getmintsinblocks`
- `reservebalance`
- `getpoolinfo`

### Newly introduced commands

The following new commands have been added to the RPC interface:
- `logging` Gets and sets the logging configuration.

  When called without an argument, returns the list of categories that are currently being debug logged.<br>
  When called with arguments, adds or removes categories from debug logging.<br>
  E.g. `logging "[\"all\"]" "[\"http\"]""`


*4.1.0* Change log
==============

Detailed release notes follow. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### GUI
 - #1044 `9d259c93c7` [Qt] Define QT_NO_KEYWORDS (Fuzzbawls)
 - #1047 `ebe1fcbce4` [Qt] Add more information to settings info panel (Fuzzbawls)
 - #1049 `521d13b6f0` [Qt] Switch to newer connect syntax (Fuzzbawls)
 - #1286 `b0e43379ee` [GUI] Set width and default minimum scrollbar height (Mrs-X)
 - #1289 `4c39ad1ec6` [GUI] Windowtitle parameter fix (Mrs-X)
 - #1297 `5e407c71cc` [GUI] Restore address list when switching panes in CSwidget (random-zebra)
 - #1298 `5dfad15c57` [Model][Performance] Unnecessary double cs_wallet and cs_main lock. (furszy)
 - #1301 `af90f927a0` [ClientModel] Remove polling based chain height update entirely. (furszy)
 - #1306 `65ba128634` [GUI] Hide privacy widget when the wallet has no zPIV balance (furszy)
 - #1316 `bcb04a44a1` [Trivial] fix typo "recomended" in sendcustomfee dialog (random-zebra)
 - #1323 `e2c07184f9` [Qt] Hide zPIV balances when they are zero (Fuzzbawls)
 - #1332 `a5be177025` [GUI] Do not update the GUI so often when reindex/import is being executed. (furszy)
 - #1339 `1716e6e835` [GUI] Tor topbar icon status. (furszy)
 - #1353 `ff7b460ef4` [GUI] Export csv files. (furszy)
 - #1354 `8c31fed2f5` [Trivial] Minor showed amount fix (NoobieDev12)
 - #1357 `85497fed21` [Qt] Replace deprecated Qt methods (Fuzzbawls)
 - #1361 `0a613c0a5e` [GUI] Dashboard, include owner cold stakes in the chart (furszy)
 - #1377 `5a600bab11` [Trivial] Fix typo in settingsbittoolwidget.cpp (RottenCoin)
 - #1385 `be05f52000` [GUI] Don't change selected address after editing its label (random-zebra)
 - #1387 `cc072687db` [GUI] Add Unlock/Relock context flow (random-zebra)
 - #1388 `c80688e02a` [GUI] Customize the timeout of the SnackBar based on its message length (random-zebra)
 - #1389 `f80888b770` [GUI] MasternodeWidget-Wizard bugfixes (random-zebra)
 - #1391 `2bdc9f8689` [GUI] Spend cold-stake delegations (random-zebra)
 - #1392 `da2f6e56f8` [GUI] Accept dialogs with ENTER (random-zebra)
 - #1393 `541a688bee` [GUI] Customizable sorting for address books (random-zebra)
 - #1396 `b56172a01b` [GUI] Topbar MNs sync phase status. (furszy)
 - #1400 `f1206ed87d` [Trivial] Rewording of Custom Fee dialog text (NoobieDev12)
 - #1402 `023e7719f2` [GUI][Bug] Fix max decimals in sendcustomfeedialog (random-zebra)
 - #1403 `de5fb73775` [GUI] SendWidget: automatically set focus on last recipient entry (random-zebra)
 - #1404 `28bc0e3104` [GUI] SendMultiRow: clear address label when address is changed (random-zebra)
 - #1405 `d8663a8410` [GUI] Upgrade to HD wallet functionality. (furszy)
 - #1406 `ca26229afc` [GUI] Save custom fee selected (random-zebra)
 - #1408 `8bd8c2ca93` [GUI][Model] Save cache BlockIndex tip in the model (random-zebra)
 - #1413 `4fdd3225b3` [GUI] Explicit cast from quint32 to bool in filterAcceptsRow (random-zebra)
 - #1423 `ca48a453eb` [GUI][Trivial] Fix button size in welcomecontentwidget (random-zebra)
 - #1424 `592aa52fca` [GUI] One-click export to file (random-zebra)
 - #1425 `be6f1279b6` [Trivial][GUI] Add more room to contacts dropdown (random-zebra)
 - #1428 `1ba03cc688` [GUI][Trivial] Emphasize warning better (NoobieDev12)
 - #1429 `623821c799` [GUI] Dark theme (furszy)
 - #1430 `1907a80938` [Trivial][GUI] Fix capsLabel color (make it text-warning) (random-zebra)
 - #1431 `01651b6c63` [GUI] Automatic wallet backup after upgrade to HD (random-zebra)
 - #1436 `98e6867e0c` [Trivial] Rewording of remote masternode data export notification (NoobieDev12)
 - #1443 `0b0c8267a6` [GUI] Removing unused DenomGenerationDialog. (furszy)
 - #1447 `ec666ec8d8` [GUI] Add address-label to sendconfirm-popup (random-zebra)
 - #1454 `110aa9c83d` [GUI] Settings widget (furszy)
 - #1464 `0bbf18ee1a` [GUI][Model] Fix staking address validation (random-zebra)
 - #1468 `8860401a75` [GUI] Less GUI locks (furszy)
 - #1469 `7521d6d193` [GUI][Cleanup] Coin Control Cleanup (random-zebra)
 - #1470 `02ede334f2` [GUI] Mark delegated UTXOs in CoinControl (random-zebra)
 - #1473 `ca7f351350` [GUI] Hide charts container when not USE_QTCHARTS (random-zebra)
 - #1475 `3dddd25def` [GUI] Hide charts at startup or at runtime (random-zebra)
 - #1477 `6879786153` [GUI][Trivial] ColdStakingWidget: fix containerSend margins (random-zebra)
 - #1479 `c9c618e365` [BUG][GUI] Fix MasternodeWidget StartAll (furszy)
 - #1481 `8e9214014b` [GUI][BugFix] Tor topbar button text. (furszy)
 - #1487 `ac7a5b302f` [GUI] ClientModel cacheTip sync with back-end. (furszy)
 - #1489 `e4d6c26f61` [GUI] Add missing error notification in encrypt wallet. (furszy)
 - #1494 `3ea8c577d4` [Qt] Convert leftover connects to Qt5 syntax (Fuzzbawls)
 - #1499 `5e6c05305e` [GUI] Settings console (furszy)
 - #1501 `417e8cb73c` [GUI] Update MNs count every 40 seconds. (furszy)
 - #1503 `66f523d46f` [GUI] Fix cold staking owner dropdown position. (furszy)
 - #1508 `104db7d7f3` [BUG] Prevent StartAll from starting mns with immature collateral (random-zebra)
 - #1509 `13c4302087` [GUI] Correct the reference to ColdStakingWidget address (PWRB-Project)
 - #1510 `5a4b0f9a70` [GUI] Fix text cut-off in sendchangeaddressdialog (JSKitty)
 - #1515 `24bc866346` [GUI] Back port latest MacOS dock icon handler. (furszy)
 - #1522 `ab353d9848` [Qt] Fixup filter dropdown localizations (Fuzzbawls)
 - #1523 `e92c8ed86a` [Trivial] [GUI] Fix masternodeswidget snackbar typos (JSKitty)
 - #1527 `6e1745be85` [GUI] Warn about change address not belonging to the wallet. (furszy)
 - #1529 `f05a451021` [Trivial] Fix compiler warning in mousePressEvent. (furszy)
 - #1530 `b7c0c0e31d` [GUI] Don't log to console by default. (furszy)
 - #1535 `7d25604239` [Qt] Don't translate dummy strings in mnrow (Fuzzbawls)
 - #1543 `440c8f514b` [GUI][Bug] CoinControl: mark delegated after setting checked state (random-zebra)
 - #1545 `7fdf25b4f7` [GUI] MasternodeWizard validations (random-zebra)
 - #1551 `8b25a1eaf5` [GUI][Bug] Reset custom change address (random-zebra)
 - #1556 `5ba10f0d58` [GUI] CoinControlDialog remove duplicate esc button (furszy)
 - #1574 `18bce1bfaf` [Bug][GUI] SendCustomFee: reset checkbox on clearAll (random-zebra)
 - #1576 `e99a18e410` [GUI] SendCustomFeeDialog: prevent user from saving insane fees (random-zebra)
 - #1577 `2205e3d302` [RPC][GUI][Bug] Disable/Hide multisend (random-zebra)
 - #1578 `c064baaf6e` [GUI][Bug] Fix transaction details output-index (random-zebra)
 - #1581 `55c9236429` [GUI] Do not create new SettingsMultisendWidget (furszy)
 - #1588 `7694d5fc6d` [GUI][Bug] Fix editing of CS address labels (Fuzzbawls)
 - #1589 `7694d5fc6d` [GUI][Bug] Don't clear address label during send address validation (Fuzzbawls)
 - #1590 `1fc629be86` [GUI] Update translations from Transifex for 4.1 (Fuzzbawls)
 - #1594 `e001ddf106` [GUI][Bug] Reconnect CS owner address edit-label action (Fuzzbawls)
 - #1595 `7779ab1194` [GUI][Bug] Fix "Select all" / "Unselect all" logic in coincontrol (random-zebra)
 - #1599 `72595d7692` [GUI][Bug] Fix language selection invalidly stored (furszy)

### RPC Interface
 - #1299 `523db49f49` [Trivial][Regtest][RPC] generate call failing properly when the wallet is locked (furszy)
 - #1321 `d382a249be` [RPC][Tests] Don't throw when generate doesn't create all blocks (PoS) (random-zebra)
 - #1343 `735d8ddb4d` [Bug] nStakeSplitThreshold: division by zero (random-zebra)
 - #1368 `d610d013dd` [RPC] Expand getstakingstatus output (random-zebra)
 - #1381 `88256004a3` [RPC][Bug] Fix masternodecurrent: return next winner, not rank-1 mn (random-zebra)
 - #1399 `eb8cb624f9` [RPC] Upgrade to HD wallet. (furszy)
 - #1451 `1fa5156a97` [RPC] Add logging RPC (random-zebra)
 - #1497 `fd77102258` [Trivial][RPC] Remove references to old command list-conf (random-zebra)

### Wallet
 - #1277 `d870d5c003` [Wallet] Refactor MintableCoins (random-zebra)
 - #1300 `0e3644d531` [Wallet] Add function CMerkleTx::IsInMainChainImmature (random-zebra)
 - #1327 `d58fd6fee1` [Wallet] HD Wallet - v2. (furszy)
 - #1345 `abdc0e762e` [Wallet][RPC][GUI] nStakeSplitThreshold as CAmount (random-zebra)
 - #1356 `d6298c5fa0` [Wallet][GUI] Set default stake-split threshold to 500 (random-zebra)
 - #1369 `bb9b762bb1` [Wallet] Fix staking balance calculation (random-zebra)
 - #1373 `5d004d514c` [Wallet] Remove reserve balance (random-zebra)
 - #1382 `df2db0d5c6` [Wallet] Don't initialize zpivwallet on first run (Fuzzbawls)
 - #1401 `e1585f7609` [Wallet][Bug] Fix ScriptPubKeyMan::CanGetAddresses (random-zebra)
 - #1411 `3c34c34fd1` [Wallet][Bug] Fix ScriptPubKeyMan::CanGenerateKeys (random-zebra)
 - #1458 `8d8050fa6d` [Wallet][Bug] Fix min depth requirement for stake inputs in AvailableCoins (random-zebra)
 - #1474 `840821321c` [Wallet] Fix wrong Lock/Unlock zWallet method call. (furszy)
 - #1485 `d43d5fa1f6` [Wallet] Fix BIP38 import crashing on empty DecKey (JSKitty)
 - #1495 `caa91b08fd` [Wallet] Clean wrong pwalletMain inside wallet class. (furszy)
 - #1498 `ebfa5979a6` [Wallet] Initialize zwalletMain to prevent memory access violations. (furszy)
 - #1520 `b92584a482` [Wallet] remove unused code, remove cs_main lock requirement (furszy)
 - #1537 `dabb5168f1` [Wallet] Cleanup invalid IsMasternodeReward method in OutPoint primitive. (furszy)
 - #1565 `7fb724ffe4` [Bug] Simpler nTimeSmart computation (random-zebra)
 - #1575 `f16125b83d` [Bug][Wallet] Fix insane fees (random-zebra)

### Core Systems
 - #1249 `fbcc5305d6` [Script] Optimize and Cleanup CScript::FindAndDelete (Akshay)
 - #1259 `cf1bab30d5` [Core] Remove StakeV1 (random-zebra)
 - #1302 `e9ceb6daf9` [Refactor] Move CBlockFileInfo and CValidationState out of main (Fuzzbawls)
 - #1308 `ca912fc823` [zPIV] Public coin, a super for-each removed (furszy)
 - #1319 `fc6d9514f4` [Refactor] Move CDiskTxPos/CBlockUndo to txdb.h/undo.h respectively (barrystyle)
 - #1320 `563d5c2515` [Refactor] Move transaction checks out to consensus/tx_verify.cpp (barrystyle)
 - #1325 `91566195ee` Add WITH_LOCK macro: run code while locking a mutex. (furszy)
 - #1328 `6143e95f98` [Refactor] Introduce Consensus namespace (barrystyle)
 - #1331 `484a98673f` [Core] CheckColdStakeFreeOutput: skip SPORK checks if mnsync incomplete (random-zebra)
 - #1333 `6bb09adfbb` [Backport] Wait locking for genesis connection. (furszy)
 - #1335 `535baaacd7` Inlining sync.cpp with latest upstream (furszy)
 - #1336 `f30074a123` Sync Upgrades, part 2 (furszy)
 - #1337 `c5fc9cc7a3` Sync upgrade, part 3 (furszy)
 - #1338 `d00ac8c3d2` [Core] CBlockIndex cleanup - Bump client version to 4.0.99.1 (random-zebra)
 - #1341 `c992fc1523` [Refactor] ChainParams::consensus - Part 1 (random-zebra)
 - #1342 `dc530c072f` [Refactor] Move zerocoin checks out of main (Fuzzbawls)
 - #1344 `e4041b1631` [Refactor] ChainParams::consensus - Part 2 (random-zebra)
 - #1359 `9869a384e2` [Chain][Bug] Fix mapZerocoinSupply recalculation (random-zebra)
 - #1364 `f4339f4621` [Refactor][PoS] Kernel Refactoring (random-zebra)
 - #1375 `41aae2d589` [Init] Do parameter interaction before creating the UI model (Jonas Schnelli)
 - #1376 `ac4ab94dd8` [Init] Combine common error strings so translations can be shared (Luke Dashjr)
 - #1395 `b3fcbea5b1` Replace uint256/uint160 with opaque blobs where possible (First) (furszy)
 - #1407 `678d3ed933` [Consensus] CheckColdStakeFreeOutput when mnsync not complete (random-zebra)
 - #1409 `8c112d3ac0` [MN] Refactor to the `mapMasternodesLastVote` map. (furszy)
 - #1418 `87e3dcddef` [Consensus] Remove IsSuperMajority (random-zebra)
 - #1445 `9896879fff` [Refactor] Init wallet parameters interaction (furszy)
 - #1450 `cb6d15a6ac` [Util] Buffer log messages and explicitly open logs (random-zebra)
 - #1457 `841f702d13` [Bug] Fix g_best_block initialization in ABC (random-zebra)
 - #1465 `1c5d9c641e` [Bug] Fix height_start_StakeModifierV3 (Stamek)
 - #1502 `0c8950a8ec` [Core] Reduce CBlockIndex RAM usage (random-zebra)
 - #1504 `4689f7e9a6` [BUG] Fix ambiguous call to distance in ParseAccChecksum (random-zebra)
 - #1517 `c4834eb1b6` [Shutdown] remove PID file at the very end. (furszy)

### P2P Protocol and Network Code
 - #1307 `31a56e9cf9` [Net] Do not launch the clock warning dialog like crazy. (furszy)
 - #1360 `aee230135f` [Net] Split DNS resolving functionality out of net structures (Cory Fields)
 - #1363 `7c78ece64d` [Net] Banlist updates (Philip Kaufmann)
 - #1367 `82b4147a30` [Net] Max block locator entries size. (furszy)
 - #1372 `59fdfee134` [P2P] Remove p2p alert system (random-zebra)
 - #1378 `2055163092` [Net] Fix de-serialization bug where AddrMan is corrupted after exception (Fuzzbawls)
 - #1452 `a86ee0f271` [Net] Add support for dnsseeds with option to filter by servicebits (Jonas Schnelli)
 - #1453 `195d46690c` [Net] Addrman offline attempts (Gregory Maxwell)
 - #1460 `18fa545831` [Net] Do not print an error on connection timeouts through proxy (furszy)
 - #1463 `976f46d6e1` [Net] Protocol update enforcement for 70919 (random-zebra)
 - #1484 `0a4072e4fc` [Net] Fix resource leak in ReadBinaryFile (practicalswift)
 - #1492 `86ecb08159` [Net] Relay actual peers instead of localhost on getaddr (akshaynexus)
 - #1538 `38f848ecb1` [P2P] Update hard coded seed nodes (Fuzzbawls)
 - #1591 `8a96306de6` [P2P] Removing dead seeder (furszy)

### Build Systems
 - #1256 `05bd016774` [Build][Scripts] Update translation flow (Fuzzbawls)
 - #1324 `586a051c7f` [Build] Remove Windows 32 bit build. (furszy)
 - #1351 `5ad9c81acc` [Build] Bump minimum required Qt version to 5.5.1 (Fuzzbawls)
 - #1358 `1517dbe271` [Build] Cleanup Qt m4 file (Fuzzbawls)
 - #1390 `d84d6bea2e` [Build] Add flag for enabling thread_local (Fuzzbawls)
 - #1435 `79c539acf7` [Travis][Build] Restore macOS Travis tests (Fuzzbawls)
 - #1448 `ab0be2f1e4` [CMake] Fix missing file error (Fuzzbawls)
 - #1483 `f79a7026d0` [Depends] Update OpenSSL download URL (Fuzzbawls)
 - #1493 `5df2b6767b` [Depends] Include qt-gif plugin. (furszy)
 - #1536 `be79988a8f` [Build] Disable apt-cacher for Windows WSL gitian setup (Fuzzbawls)

### Testing Systems
 - #1278 `805800164e` [Tests][Bug] Fix staking status in generate_pos() (random-zebra)
 - #1295 `e7cd10434f` [Tests] Set '-staking' disabled by default on RegTest (random-zebra)
 - #1312 `f860860e4d` [Tests] Use V2 stake modifiers on regtest (random-zebra)
 - #1410 `0b5fef0376` [Tests] Run tests on pre-HD wallets (random-zebra)
 - #1434 `c2ddd267f5` [QA] Continuous Testing Environment for legacy pre-HD wallets (random-zebra)
 - #1442 `a5288eca97` [Tests] Remove non-determinism which is breaking net_tests (random-zebra)
 - #1446 `2f9bfc84b0` [Test] Wallet testing setup. (furszy)
 - #1472 `46585f7b62` [Tests][Refactor] Remove connect_nodes_bi (random-zebra)
 - #1550 `f246e104e5` [QA] Avoid printing to console during cache creation (random-zebra)
 - #1580 `3d36502c3a` [QA][Bug] Shorter wallet_basic.py functional test (random-zebra)

### Logging
 - #1437 `8a6f5147c9` [Util] LogAcceptCategory: use uint32_t rather than sets of strings (random-zebra)
 - #1438 `37d53b777c` [Trivial] Remove spammy log in CheckSignature (random-zebra)
 - #1439 `eb1ed66c2e` [Core] Add -debugexclude option (random-zebra)
 - #1449 `9ef9f5dea1` [Util] tinyformat / LogPrint backports (random-zebra)
 - #1455 `b26dbc4e55` [Init] Add `-debuglogfile` option (random-zebra)
 - #1459 `c94f2412af` [Util] Refactor logging code into a global object (random-zebra)

### Cleanup
 - #1247 `3e1233cccb` [Cleanup] Remove automint and automint addresses (Fuzzbawls)
 - #1254 `cc547e7927` [Cleanup] ZLNP code removed entirely. (furszy)
 - #1276 `e5d6fdfd38` [Tests][Cleanup][Trivial] Remove legacy zerocoin spends tests (random-zebra)
 - #1281 `a293072cdb` [Trivial][Cleanup] Remove extra checks before GetBlocksToMaturity (random-zebra)
 - #1282 `76f29fccf3` [Trivial][Cleanup] Add IsRegTestNet() function in chainparams (random-zebra)
 - #1290 `2ceeb2cca0` [zPIV][Cleanup] Zerocoin Cleanup 1: remove Accumulators values (random-zebra)
 - #1291 `16d7dac5f7` [zPIV][Cleanup] Zerocoin Cleanup 2: remove CZPivStake class (random-zebra)
 - #1293 `28e0048b3e` [zPIV][Cleanup] Zerocoin Cleanup 3: remove old ZK proofs (random-zebra)
 - #1314 `185194bd7e` [zPIV][Cleanup] Zerocoin Cleanup 4: further wallet cleaning (random-zebra)
 - #1322 `5bd387e18d` [zPIV][Cleanup] Zerocoin Cleanup 5: further main.cpp cleaning (random-zebra)
 - #1326 `6bbd575860` [zPIV][Cleanup] Zerocoin Cleanup 6: Remove zerocoin mint checks (random-zebra)
 - #1330 `b766048068` [Cleanup] Remove stale UNITTEST network (Fuzzbawls)
 - #1340 `57cf8a67bd` [Cleanup] Remove MineBlocksOnDemand function in chainparams (random-zebra)
 - #1352 `fe60594c4a` [Cleanup] Remove unused blockexplorer GUI files (Fuzzbawls)
 - #1412 `35c58d8ef7` [Cleanup] Remove walletbroadcast and resendwallettransactions tests (random-zebra)
 - #1456 `42567f14b2` [Trivial] Cleaning not used pCheckpointCache. (furszy)
 - #1461 `0a63983ac2` [Cleanup] Remove old governancepage and proposalframe code (Akshay)
 - #1467 `aa50351b18` [Cleanup] Remove redundant and unused code (Fuzzbawls)
 - #1490 `dfda1b80af` [Cleanup] Nuke obfuscation from orbit (Fuzzbawls)
 - #1507 `172387aad5` [Trivial] Remove leftover temporary comment (random-zebra)
 - #1513 `ae4fde3b3a` [Trivial] Remove CMasternode::SliceHash (random-zebra)

### Documentation
 - #1573 `f973fb6fd6` [Doc] Note that v3 onion addresses are not supported (Fuzzbawls)

### Scripts and Tools
 - #1433 `f5f6c3cd92` [Scripts] Only fetch translations for high-completion locals (Fuzzbawls)

### Misc
 - #1583 `7fdf29f816` Update copyright headers for files changed in 2020 (Fuzzbawls)

## Credits

Thanks to everyone who directly contributed to this release:
- Cory Fields
- EthanHeilman
- Fuzzbawls
- Gregory Maxwell
- JSKitty
- Jonas Schnelli
- Luke Dashjr
- Mrs-X
- NoobieDev12
- PWRB-Project
- Philip Kaufmann
- RottenCoin
- Stamek
- akshaynexus
- barrystyle
- furszy
- practicalswift
- random-zebra

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.