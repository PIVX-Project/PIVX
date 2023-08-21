hemis Core version *3.1.0* is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>


Mandatory Update
==============

hemis Core v3.1.0 is a mandatory update for all users. This release contains new consensus rules and improvements that are not backwards compatible with older versions. Users will have a grace period of one week to update their clients before enforcement of this update is enabled.

Users updating from a previous version after Tuesday, May 8, 2018 12:00:00 AM GMT will require a full resync of their local blockchain from either the P2P network or by way of the bootstrap.

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

**Currently there are issues with the 3.x gitian release on MacOS version 10.13 (High Sierra), no reports of issues on older versions of MacOS.**

 
Notable Changes
==============

zHMS Updates
--------------

### zHMS Staking

zHMS Staking is here! zHMS staking will be activated on the morning of the 8th of May 2018. With the release of zHMS staking, there are effectively 2 versions of zHMS, zHMS minted on the 3.0.6 hemis wallet or lower, and zHMS minted on hemis wallet version or higher. New features in this release will require the use of zHMS v2, zHMS minted on this wallet release 3.1.0 or later. If you currently hold zHMS v1 and wish to take advantage of zHMS staking and deterministic zHMS, you will need to spend the zHMS v1 to yourself and remint zHMS v2.
Note: To find your zHMS version, click the privacy tab, then the zHMS Control button then expand the arrows next to the desired denomination.


### Deterministic zHMS Seed Keys

zHMS is now associated with a deterministic seed key. With this seed key, users are able to securely backup their zHMS outside of the wallet that the zHMS had been minted on. zHMS can also be transferred from wallet to wallet without the need of transferring the wallet data file.


### Updated zHMS minting

zHMS minting now only requires 1 further mint (down from 2) to mature. zHMS mints still require 20 confirmations.  Mints also require that the 'second' mint is at least two checkpoints deep in the chain (this was already the case, but the logic was not as precise).


### zHMS Search

Users will now have the ability to search the blockchain for a specific serial # to see if a zHMS denomination has been spent or not.



HMS/zHMS Staking and Masternode Rewards
--------------

### HMS, zHMS and Masternode Payment Schedule

To encourage the use of zHMS and increase the hemis zerocoin anonymity set, the hemis payment schedule has been changed to the following:

If a user staking zHMS wins the reward for their block, the following zHMS reward will be: 
- 3 zHMS (3 x 1 denominations) rewarded to the staker, 2 HMS rewarded to the masternode owner and 1 HMS available for the budget. This is a total block reward of 6 HMS, up from 5.

If a user staking HMS wins the reward, the following amounts will be rewarded: 
- 2 HMS to the HMS staker, 3 HMS to the Masternode owner and 1 HMS available for the budget. This is a total block reward of 6 HMS, up from 5.


### Return change to sender when minting zHMS

Previously, zHMS minting would send any change to a newly generated "change address". This has caused confusion among some users, and in some cases insufficient backups of the wallet. The wallet will now find the contributing address which contained the most HMS and return the change from a zHMS mint to that address.


User Experience
--------------

### Graphical User Interface

The visual layout of the hemis Qt wallet has undergone a near-complete overhaul.
A new 'vertical tab' layout is now being used instead of the prior 'horizontal tab' layout, as well as a completely new icon set.
The overview tab has been simplified greatly to display only balances that are active or relevant, zero-balance line items are hidden by default to avoid clutter.


### Wallet Options

There have been a number of changes to the tasks that you are able to perform from the wallet options. Users will now have the ability to do the following: 
-	Enable and disable the auto zHMS minting feature. This is enabled by default and the enablezeromint=0 setting in the hemis.conf file will overwrite the GUI option.
-	The percentage of autominted zHMS can now be set from 1 to 100, changed from 10 – 100.
-	The stake split threshold can now be set VIA the wallet options. This setting is an advanced feature for those wishing to remain staking regular HMS.
-	“Unlock for staking and anonymization only” is now selected by default when unlocking the wallet from the User Interface


### In-wallet Ban Management

Peer bans are now manageable through the Peers tab of the tools window. Peers can be banned/unbanned at will without the need to restart the wallet client. No changes have been made to the conditions resulting in automatic peer bans.


Backup to external devices / locations
--------------

### Summary

 The hemis wallet can now have user selected directories for automatic backups of the wallet data file (wallet.dat). This can be set by adding the following lines to the hemis.conf file, found in the hemis data directory.
- backuppath = <directory / full path>
- zpivbackuppath = <directory / full path>
- custombackupthreshold = <backup limit>
Note: System write permissions must be appropriate for the location the wallet is being saved to.

* Configured variables display in the _Wallet Repair_ tab inside the _Tools Window / Dropdown Menu_
* Allows for backing up wallet.dat to the user set path, simultaneous to other backups
* Allows backing up to directories and files, with a limit (_threshold_) on how many files can be saved in the directory before it begins overwriting the oldest wallet file copy.


### Details:

* If path is set to directory, the backup will be named `wallet.dat-<year>-<month>-<day>-<hour>-<minute>-<second>`
* If zHMS backup, auto generated name is `wallet-autozpivbackup.dat-<year>-<month>-<day>-<hour>-<minute>-<second>`
* If path set to file, backup will be named `<filename>.dat`
* walletbackupthreshold enables the user to select the maximum count of backup files to be written before overwriting existing backups.


### Example:

* -backuppath=/\<mynewdir>/        
* -walletbackupthreshold=2

Backing up 4 times will result as shown below


                date/time
    backup #1 - 2018-04-20-00-04-00  
    backup #2 - 2018-04-21-04-20-00  
    backup #3 - 2018-04-22-00-20-04  
    backup #4 - 2018-04-23-20-04-00  
    
    1.
        /<mynewdir>/
            wallet.dat-2018-04-20-00-04-00
    2.
        /<mynewdir>/
            wallet.dat-2018-04-20-00-04-00
            wallet.dat-2018-04-21-04-20-00
    3.
        /<mynewdir>/
            wallet.dat-2018-04-22-00-20-04
            wallet.dat-2018-04-21-04-20-00
    4.
        /<mynewdir>/
            wallet.dat-2018-04-22-00-20-04
            wallet.dat-2018-04-23-20-04-00
            


hemis Daemon & Client (RPC Changes)
--------------

### RPC Ban Management

The hemis client peer bans now have additional RPC commands to manage peers. Peers can be banned and unbanned at will without the need to restart the wallet client. No changes have been made to the conditions resulting in automatic peer bans. New RPC commands: `setban`, `listbanned`, `clearbanned`, and `disconnectnode`


### Random-cookie RPC authentication

When no `-rpcpassword` is specified, the daemon now uses a special 'cookie' file for authentication. This file is generated with random content when the daemon starts, and deleted when it exits. Its contents are used as authentication token. Read access to this file controls who can access through RPC. By default it is stored in the data directory but its location can be overridden with the option `-rpccookiefile`.
This is similar to Tor's CookieAuthentication: see https://www.torproject.org/docs/tor-manual.html.en 
This allows running hemisd without having to do any manual configuration.


### New RPC command
`getfeeinfo`

This allows for a user (such as a third party integration) to query the blockchain for the current fee rate per kb, and also get a suggested rate per kb for high priority tx's that need to get added to the blockchain asap.


### New RPC command 
`findserial`

Search the zerocoin database for a zerocoinspend transaction that contains the given serial. This will be a helpful tool for the hemis support group, which often times sees users say "I didn't spend that zHMS". This RPC call allows for support to grab the serial, and then find the spend tx on the chain.


### New RPC commands 
`createmasternodebroadcast`

`decodemasternodebroadcast`

`relaymasternodebroadcast`

A new set of rpc commands masternodebroadcast to create masternode broadcast messages offline and relay them from online node later (messages expire in ~1 hour). 


Network Layer 2 Changes (Proposals / Budgets / SwiftX)
--------------

### Monthly Budget Increase

As voted on by the hemis masternodes, the monthly budget available to be utilised has been increased to 42,000 HMS / month. This HMS only has the opportunity to be raised once per month (paid to winning proposals) with any unused HMS not created by the blockchain.

### Budget Finalization Fee

The hemis finalization fee for successful proposals has now been reduced, this fee is now 5 HMS down from 50 HMS. The total fee outlay for a successful proposal is now a total of 55 HMS.


### SwiftX Raw Transactions

 When creating a raw transaction, it is now possible to create the transaction as a SwiftX transaction. See the updated help documentation for the `createrawtransaction` RPC command.

Technical Changes
--------------

### Migration to libevent based http server

The RPC and REST interfaces are now initialized and controlled using standard libevent instead of the ad-hoc pseudo httpd interface that was used previously. This change introduces a more resource friendly and effective interface.


### New Notification Path 
`blocksizenotify`

A new notification path has been added to allow a script to be executed when receiving blocks larger than the 1MB legacy size. This functions similar to the other notification listeners (`blocknotify`, `walletnotify`, etc).


### Removed Growl Support

Growl hasn't been free nor needed for many years. MacOS versions since 10.8 have the OS notification center, which is still supported after this.


### Autocombine changes

The autocombine feature was carrying a bug leading to a significant CPU overhead when being used. The function is now called only once initial blockchain download is finished. It's also now avoiding to combine several times while under the threshold in order to avoid additional transaction fees. Last but not least, the fee computation has been changed and the dust from fee provisioning is returned in the main output.


### SOCKS5 Proxy bug

When inputting wrong data into the GUI for a SOCKS5 proxy, the wallet would crash and be unable to restart without accessing hidden configuration. This crash has been fixed.

Minor Enhancements
--------------

-	Enforced v1 zHMS spends to require a security level of 100
-	Updates to zHMS spends to avoid segfaults
-	Updates to configuration will now reflect on the privacy tab
-	Fixed a  bug that would not start masternodes from the hemis-Qt masternodes tab
-	Updated hemis-Qt tooltips
-	Icon added to the wallet GUI to reflect the status of autominting (active / inactive)
-	Updated errors causing the blockchain to corrupt when experiencing unexpected wallet shutdowns
-	Updated RPC help outputs & removed the deprecated obfuscation. 
-	Refactored code
-	Various bug fixes
-	Updated documentation

Further Reading: Version 2 Zerocoins
==============

Several critical security flaws in the zerocoin protocol and hemis's zerocoin implementation have been patched. Enough has changed that new zerocoins are distinct from old zerocoins, and have been labelled as *version 2*. When using the zHMS Control dialog in the QT wallet, a user is able to see zHMS marked as version 1 or 2.

zPoS (zHMS staking)
--------------

Once a zHMS has over 200 confirmations it becomes available to stake. Staking zHMS will consume the exact zerocoin that is staked and replace it with a freshly minted zerocoin of the same denomination as well as a reward of three 1 denomination zHMS. So for example if a 1,000 zHMS denomination is staked, the protocol replaces that with a fresh 1,000 denomination and three1 denomination zHMSs.

Secure Spending
--------------

Version 1 zerocoins, as implemented by [Miers et. al](http://zerocoin.org/media/pdf/ZerocoinOakland.pdf), allow for something we describe as *serial trolling*. Spending zerocoins requires that the spender reveal their serial number associated with the zerocoin, and in turn that serial number is used to check for double spending. There is a fringe situation (which is very unlikely to happen within hemis's zerocoin implementation due to delayed coin accumulation) where the spender sends the spending transaction, but the transaction does not immediately make it into the blockchain and remains in the mempool for a long enough duration that a *troll* has enough time to see the spender's serial number, mint a new zerocoin with the same serial number, and spend the new zerocoin before the original spender's transaction becomes confirmed. If the timing of this fringe situation worked, then the original spender's coin would be seen as invalid because the troll was able to have the serial recorded into the blockchain first, thus making the original spender's serial appear as a double spend.

The serial troll situation is mitigated in version 2 by requiring that the serial number be a hash of a public key. The spend requires an additional signature signed by the private key associated with the public key hash matching the serial number. This work around was conceived by Tim Ruffing, a cryptographer that has studied the zerocoin protocol and done consulting work for the ZCoin project.

Deterministic Zerocoin Generation
--------------

Zerocoins, or zHMS, are now deterministically generated using a unique 256 bit seed. Each wallet will generate a new seed on its first run. The deterministic seed is used to generate a string of zHMS that can be recalculated at any time using the seed. Deterministic zHMS allows for users to backup all of their future zHMS by simply recording their seed and keeping it in a safe place (similar to backing up a private key for HMS). The zHMS seed needs to remain in the wallet in order to spend the zHMS after it is generated, if the seed is changed then the coins will not be spendable because the wallet will not have the ability to regenerate all of the private zHMS data from the seed. It is important that users record & backup their seed after their first run of the wallet. If the wallet is locked during the first run, then the seed will be generated the first time the wallet is unlocked.

Zerocoin Modulus
--------------

hemis's zerocoin implementation used the same code from the ZCoin project to import the modulus use for the zerocoin protocol. The chosen modulus is the 2048 bit RSA number created for the RSA factoring challenge. The ZCoin project's implementation (which hemis used) improperly imported the modulus into the code. This flaw was discovered by user GOAT from the [Civitas Project](https://github.com/eastcoastcrypto/Civitas/), and reported to hemis using the bug bounty program. The modulus is now correctly imported and hemis's accumulators have been changed to use the new proper modulus.


*3.1.0* Change log
--------------

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core Features
 - #582 `cb1632520` [Core] zHMS v2: zPoS dzHMS ezHMS external backup and budget fixes (too many to list)
 - #558 `c7e6f0f7f` [Core] Remove Gitan-OSX warning for High Sierra builds (Mrs-X)
 - #523 `926c073ea` [Core] Give high priority to zerocoinspends to make it into the next block. (presstab)
 - #535 `5e8875feb` [Core] Minor refacturing + unused variable removed (Mrs-X)
 - #526 `0e034d62f` [Core] Refactor ConnectBlock() to segregate state tracking items (presstab)
 - #513 `27cdf61f2` [Core] Fix masternode broadcast for networks != MAINNET (Mrs-X)
 - #261 `29611a5fd` [Core] Switch from local to main signals for the validation interface (Nitya Sattva)
 - #460 `ae4b6a135` [Core] (testnet-) blockchain stuck at block 325000 (Mrs-X)
 - #428 `3de6b6f3e` [Core] Wipe zerocoin DB on reindex. (presstab)
 - #447 `8d3407ae4` [Consensus] Fix compilation with OpenSSL 1.1 (warrows)
 - #551 `94b9bc937` [Consensus] Require standard transactions for testnet (Fuzzbawls)

### Build System
 - #532 `9071bfe2f` [Depends] Update depends package versions. (Fuzzbawls)
 - #402 `a471f7020` [Build] Revert #402 "Change git info in genbuild.sh" (Fuzzbawls)
 - #541 `2f6b58cf1` [Build] Fix typo in Qt test makefile (Fuzzbawls)
 - #536 `5c44f40e9` [Build] Update build system from upstream (Fuzzbawls)
 - #480 `72d8c7d67` [Build] compile/link winshutdownmonitor.cpp on Windows only (Fuzzbawls)
 - #461 `55ec19af0` [Build] libevent-dev dependency added to build-documentation (Mrs-X)
 
### P2P Protocol and Network Code
 - #542 `61156def7` [Network] Remove vfReachable and modify IsReachable to only use vfLimited. (Patrick Strateman)

### GUI
 - #572 `d9b23fe60` [Qt] Refresh zHMS balance after resetting mints or spends (warrows)
 - #571 `1c8e7cb7b` [Qt] Update privacy tab info about zeromint on config change (warrows)
 - #568 `f226de09e` [Qt] Connect automint icon to the UI automint setting change (warrows)
 - #566 `84f43857c` [Qt] Add automint status bar icon (Fuzzbawls)
 - #567 `e12914d85` [Qt] Optimize PNGs (Fuzzbawls)
 - #537 `653115e9b` [Qt] Standardize and clean up new UI elements (Fuzzbawls)
 - #538 `24f581842` [Qt] Fix warning dialog popup for the Blockchain Explorer (Fuzzbawls)
 - #529 `c649ba7e2` [Qt] Remove Growl support (Fuzzbawls)
 - #521 `fbb105a00` [Qt] Make "For anonymization and staking only" checked by default (Mrs-X)
 - #508 `2cf3be6bb` [Qt] Fix crash when inputting wrong port for network proxy (warrows)
 - #500 `4c01ba65d` [Qt] Remove duplicate code for updating address book labels. (blondfrogs)
 - #506 `ae72bf4e2` [Qt] Autoscroll to end of zHMS status output (Mrs-X)
 - #499 `6305264f2` [Qt] Send popup simplified + SwiftTX -> SwiftX (Mrs-X)
 - #490 `ba777e4ef` [Qt] Update MultiSend GUI to allow address labels (blondfrogs)
 - #483 `5b1070365` [Qt] Fixed Dynamic Screen Elements Issue for Multisig (blondfrogs)
 - #479 `818c0c79e` [Qt] Rework of overview page (warrows)
 - #478 `98ca7bc90` [Qt] Implement Ban Management in GUI Wallet (Fuzzbawls)
 - #473 `9e2ed8f0f` [Qt] Make toolbar icons bigger (Mrs-X)
 - #462 `c62eabe7b` [Qt] Consistent trx colors for Overview + Transaction tabs (Mrs-X)
 - #472 `b7929bdcf` [Qt] Minor changes and fixes (Mrs-X)
 - #467 `7099831bd` [Qt] Clear PrivacyDialog "zPiv Selected" labels after sending. (presstab)
 - #465 `d8e21774d` [Qt] Added controls to the options dialog for enable or disable auto-minting and set required level (lex-dev3)
 - #464 `59fd7d378` [Qt] setstakesplitthreshold value set in Qt GUI (lex-dev3)
 - #452 `219b68dc9` [Qt] Complete re-design of Qt-wallet (Mrs-X)
 - #440 `011408474` [Qt] Fix empty system tray menu (PeterL73)
 - #442 `248316647` [Qt] Improve checkbox visibility (PeterL73)
 
### RPC/REST
 - #562 `772160b1b` [Wallet/RPC] Add argument to mint zerocoin from specific UTXO (warrows)
 - #539 `b6a02e9d6` [RPC] Allow watchonly coins to be shown for listunspent (blondfrogs)
 - #543 `e4522ff07` [RPC] Segfault hemis-cli getinfo while loading block index (Mrs-X)
 - #524 `2541b5001` [RPC] Add blocksizenotify command (Mrs-X)
 - #495 `4946224c1` [RPC] Show script verification errors in signrawtransaction result (Fuzzbawls)
 - #468 `00b8b8e72` [RPC/REST] Migrate to libevent based httpd server (Fuzzbawls)
 - #381 `6f91fb734` [RPC] Allow in-wallet management of peer bans (Fuzzbawls)
 - #441 `6ae17c52d` [RPC] Implement random-cookie based authentication (Fuzzbawls)
 - #443 `0a991d8ba` [RPC] Findserial (presstab)
 - #438 `f701337e7` [RPC] Fix `importzercoins` for use with UniValue (Fuzzbawls)
 - #429 `17fdde1d1` [RPC] Add getfeeinfo RPC. (presstab)
 - #423 `5dfb48ee6` [RPC] Add SwiftX to rpcrawtransaction. (presstab)
 - #170 `027f16c64` [RPC] Convert source tree from json_spirit to UniValue (Fuzzbawls)

### Wallet
 - #570 `8c8350b59` [Wallet] Add a check on zHMS spend to avoid a segfault (warrows)
 - #565 `80b803201` [Wallet] Increase valid range for automint percentage (Fuzzbawls)
 - #518 `9f6449a70` [Wallet] Combine fees when possible and fix autocombine insufficient funds (warrows)
 - #497 `f21e4456b` [Wallet] Call AutocombineDust less often (warrows)
 - #498 `bfad2a1df` [Wallet] Change the way transaction list is handled (warrows)
 - #477 `93c5f9ff5` [Wallet] Remove potential memory leak (blondfrogs)
 - #488 `d09cf916a` [Wallet] Fixes an autocombinerewards bug with above max size TXs (warrows)
 - #448 `222ef6e6b` [Wallet] Return change to sender when minting zHMS. (presstab)
 - #445 `fc570fc1e` [Wallet] Only require 1 mint to be added before spending zHMS. (presstab)
 
### Miscellaneous
 - #559 `d2b017217` [Bug] Segfault with -enableswifttx=0 / -enableswifttx=false (Mrs-X)
 - #494 `8180b7884` [Bug] remove use of variable length buffer (rejectedpromise)
 - #469 `6bd265c5e` [Tests] Fix util_tests compiler warnings with clang (Fuzzbawls)
 - #463 `22f2bb623` [Tests] Fixed compilation errors (lex-dev3)
 - #569 `aedf80b07` [Docs] OSX Build - Instructions on how to make the Homebrew OpenSSL headers visible (gpdionisio)
 - #554 `8cecb3435` [Docs] Added release notes for autocombine and proxy GUI. (warrows)
 - #533 `ad3edb9fa` [Docs] Update OSX build notes: zmq, libevent, and notes to handle possible glibtoolize error (Tim Uy)
 - #530 `fd8aa8991` [Docs] Update README.md (Warrows)
 - #528 `1c1412246` [Docs] Readme changes (Sieres)
 - #471 `18e5accb6` [Docs] Readme SeeSaw reward mechanism reference. (Dexaran)
 - #426 `3418a64d2` [Docs] Use mainnet port for example in masternode.conf file (Fuzzbawls)
 
 
## Credits

Thanks to everyone who directly contributed to this release:
- gpdionisio
- warrows
- Fuzzbawls
- Mrs-X
- presstab
- blondfrogs
- Patrick Strateman
- Tim Uy
- Sieres
- Nitya Sattva
- turtleflax
- rejectedpromise
- Dexaran
- lex-dev3
- PeterL73
- Anthony Posselli

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.
