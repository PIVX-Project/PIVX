hemis Core version 3.0.0 is now available from:

  <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including various bug fixes and
performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/hemis-project/hemis/issues>

Mandatory Update
==============

hemis Core v3.0.0 is a mandatory update for all users. This release contains new consensus rules and improvements that are not backwards compatible with older versions. Users will have a grace period of one week to update their clients before enforcement of this update is enabled.

Users updating from a previous version after the 13th of October will require a full resync of their local blockchain from either the P2P network or by way of the bootstrap.

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

**Currently there are issues with the 3.0.0 gitian release on MacOS version 10.13 (High Sierra), no reports of issues on older versions of MacOS.**


Notable Changes
===============

Zerocoin (zPIV) Protocol
---------------------

At long last, the zPIV release is here and the zerocoin protocol has been fully implemented! This allows users to send transactions with 100% fungible coins and absolutely zero history or link-ability to their previous owners.

The Zerocoin protocol allows user to convert (mint) their *PIV* to zerocoins, which we call *zPIV*. When zPIV are converted back to PIV there is no trail associated with the coins being sent, such as who originally minted those coins. Essentially the only thing the receiver of the zPIV transaction will see is that it came from the zerocoin protocol.

### zPIV Denominations
zPIV comes in specific denominations of 1, 5, 10, 50, 100, 500, 1000, and 5000. A denomination is a similar concept to paper currency, where you can hold a $100 bill but there is no available $99 bill for you to hold.

Other implementations of the zerocoin protocol only allow for spending of one denomination/zerocoin at a time. The hemis implementation of zerocoin allows users to spend any amount of zPIV they would like (with certain limitations). If a user held two denominations of 5 and they send 7.75 to a merchant, the wallet will automatically grab the two denominations of 5 and then issue 2.25 PIV in change to the spender. There is currently a limit of up to 6 individual zerocoin `coins` that can be combined into a spend, where each `coin` could be a different or similar denomination

The hemis zerocoin implementation is structured in such a way that denominations aren't needed to be known by the average user.

### Fees
zPiv transactions require more computation and disk space than typical hemis transactions, and as such require a higher transaction fee in order to prevent network spam. Fees are only charged when minting zPiv, each minted denomination is charged a flat rate of 0.01 Piv. zPiv spends are not charged a transaction fee unless the change is minted into zPiv, see the *Minting Change* section for details on fees for zPiv spends with minted change.

### Converting PIV to zPIV (*zPIV Mint*)
**GUI** - Conversion from PIV to zPIV can be done using the `Privacy Dialog` in the QT wallet. Enter the amount of PIV you would like to convert and click `Mint Zerocoin`.

**RPC** - Conversion from PIV to zPIV can be done using the `mintzerocoin` command.

**Automint** - The hemis wallet is set to convert 10% of the wallets available PIV to zPIV automatically. This can be adjusted in the GUI within the Options dialog, which allows the preferred % to be adjusted as well as the ability to set the preferred zPIV denomination that will be minted. Automint is set to be triggered when additional blocks are added to the block chain and is programmed *not* to convert your coins all at once.

Automint can be disabled by adding `enablezeromint=0` to the wallet configuration file. The preferred mint % and denomination can also be set by the configuration file using `zeromintpercentage=<n>` and `preferredDenom=<n>`.

### Converting zPIV to PIV (*zPIV Spend*)
Redeeming zPIV is done by converting it back to PIV. With the 3.0.0 software release, users are not able to send zPIV to each other directly in an atomic fashion.

**GUI** - Conversion from zPIV to PIV can be done using the `Privacy Dialog` in the QT wallet. Enter a hemis address that you would like to Pay To, enter the amount of PIV the receiver should be sent, click `Spend Zerocoin`.

**RPC** - Conversion from zPIV to PIV can be done using the `spendzerocoin` command.

### Advanced Use & Privacy Considerations
**Security Level** - When spending zPIV, a user is prompted to enter a *Security Level* choosing from 1-100. In an indirect way, the Security Level parameter allows the user to choose how many coins to obfuscate their transaction with.

A Security Level of 1 for example would take all of the minted coins in the blockchain before your mint was added to the blockchain, and would then add any coins that were minted within the next 10 blocks as well. A Security Level of 2 would do the same thing, except add the next 20 blocks worth of mints. A **Security Level of 100 will add the maximum amount of mints** up to the current end of the blockchain.

The higher the Security Level, the more computation and time it will take to spend. Although it takes longer, a level of 100 is recommended for transactions that need maximum anonymity.


**Minting Change** - The hemis implementation of the zerocoin protocol also allows the spender to choose how to receive their leftover change from a Spend transaction. For maximum anonymity it is recommended that the spender choose to receive the change in zPIV, which prevents situations where change from a zPIV spend that is redeemed in PIV is accidentally mixed with the rest of the users PIV, thus linking transactions back to a hemis address.

Since the lowest denomination of zPIV is 1, and a fee is required to mint zPIV, in most situations a high fee will be paid to mint change. The fee is the remainder of the change that cannot be converted back to zPIV. For example this would mean a spending a denomination of 10 that yields change of 6.75 in change, would issue zPIV denominations of 5 and 1 back to the sender with the remaining 0.75 that is unmintable being contributed as a fee.

**zPIV Control**
Similar to the concept of Coin Control in the QT wallet, zPIV Control allows users to select exactly which zPIV mints they would like to spend. This gives a flexibility to choose which denominations can be picked for a spend that wouldn't otherwise be available.


Tor Service Integration Improvements
---------------------

Integrating with Tor is now easier than ever! Starting with Tor version 0.2.7.1 it is possible, through Tor's control socket API, to create and destroy 'ephemeral' hidden services programmatically. hemis Core has been updated to make use of this.

This means that if Tor is running (and proper authorization is available), hemis Core automatically creates a hidden service to listen on, without manual configuration. hemis Core will also use Tor automatically to connect to other .onion nodes if the control socket can be successfully opened. This will positively affect the number of available .onion nodes and their usage.

This new feature is enabled by default if hemis Core is listening, and a connection to Tor can be made. It can be configured with the `-listenonion`, `-torcontrol` and `-torpassword` settings. To show verbose debugging information, pass `-debug=tor`.

3.0.0 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned.

### Broad Features
- #264 `15e84e5` zPIV is here! (Fuzzbawls Mrs-X Presstab Spock hemis)

### P2P Protocol and Network Code
- #242 `0ecd77f` [P2P] Improve TOR service connectivity (Fuzzbawls)

### GUI
- #251 `79af8d2` [Qt] Adjust masternode count in information UI (Mrs-X)

### Miscellaneous
- #258 `c950765` [Depends] Update Depends with newer versions (Fuzzbawls)

Credits
=======

Thanks to everyone who directly contributed to this release:
- Fuzzbawls
- Jon Spock
- Mrs-X
- hemis
- amirabrams
- presstab

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/).
