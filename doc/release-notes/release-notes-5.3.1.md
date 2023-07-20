hemis Core version v5.3.1 is now available from: https://github.com/hemis-project/hemis/releases

This is an important release for stakers and Masternodes: including several tier two synchronization, GUI and build system bug fixes.
We recommend everyone to update your node/wallet.

Please report bugs using the issue tracker at github: https://github.com/hemis-project/hemis/issues

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down, then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).

Notable Changes
==============

### GUI

* Duplicate transaction records bug fixed.
* Not visible shield and/or cold staking contacts addresses list bug fixed

### Tier Two Network

* Not advancing (stalled) tier two synchronization bug fixed.
* Spork signer not persisting new spork value to DB bug fixed.

### Build system

* "dirty" release build version name bug fixed.

5.3.1 Change log
==============

### GUI
- #2530 [GUI][BUG] remove duplicated coll. confirmation check in MNModel::data (random-zebra)
- #2491 [GUI] Translation cleanup (Fuzzbawls)
- #2551 [BUG][GUI] Fix random double/triple transaction record issue (random-zebra)
- #2557 [Trivial] Update labelSubtitleAddress text in send widget ui (random-zebra)
- #2555 [GUI] Fix not visible contacts list for shield/CS addrs (furszy)

### Consensus
- #2562 [Consensus] Add checkpoints before v5.3.1 release (random-zebra)

### Tier Two Network
- #2553 [BUG] Spork signer doesn't persist new spork value to DB (random-zebra)
- #2559 [BUG][TierTwo] Clear fulfilled requests when mnsync fails (random-zebra)
- #2560 [Bug][TierTwo] Mark peer sync request msg fulfilled only if the broadcast was performed (furszy)

### Build system
- #2525 [Build] Get rid of CLIENT_DATE (furszy)
- #2541 [Gitian][Bug] Ignore changes to relic_conf.h.in (Fuzzbawls) 
- #2546 [Snap] Fix nightly build's genbuild.sh patch (Fuzzbawls)

### RPC
- #2540 [RPC][Doc] Fix RPC/cli example in setautocombinethreshold help (random-zebra)

## Credits

Thanks to everyone who directly contributed to this release:

- furszy
- Fuzzbawls
- random-zebra
- W. J. van der Laan

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the node hosts supporting our Testnet.
