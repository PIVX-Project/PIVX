We have found that a runtime error that has caused the hemis wallet on certain operating systems to get stuck at block 908000. This build of the wallet fixes the runtime error.

If you are stuck on block 908000:
- Download the 3.0.5.1 wallet (the file names do not display the "build" version and cutoff the 1)
- Start the wallet. Block 908000 and 908001 have some computation that occurs and it is normal for them to take longer to process than a typical block would.

If you are stuck on a block before 908000:
- Download the 3.0.5.1 wallet (the file names do not display the "build" version and cutoff the 1)
- Start the wallet with `reindexaccumulators=1` in your `hemis.conf` file (you can figure out the location of your `hemis.conf` [here](https://hemis.freshdesk.com/support/solutions/articles/30000004664-where-are-my-wallet-dat-blockchain-and-configuration-conf-files-located-) )
- After starting the wallet, remove `reindexaccumulators=1` from your `hemis.conf` or else it will perform this operation each time you start your wallet.

If both of those solutions failed you can use the [blockchain snapshot](http://178.254.23.111/~pub/hemis/Daily-Snapshots-Html/hemis-Daily-Snapshots.html). If using the snapshot, please carefully follow the instructions on the snapshot page.