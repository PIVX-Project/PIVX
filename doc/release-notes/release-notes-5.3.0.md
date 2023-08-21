hemis Core version v5.3.0 is now available from: <https://github.com/hemis-project/hemis/releases>

Mandatory Update
==============

hemis Core v5.3.0 is a mandatory update for all users. This release contains new consensus rules and improvements that are not backwards compatible with older versions. Users will need to update their clients before enforcement of this update goes into effect.

Update enforcement is currently scheduled to go into effect at the following time:

```
Mainnet: block 3,014,000. ~10th September.
```
Gamemasters will need to be restarted once both the gamemaster daemon and the controller wallet have been upgraded.
If you are running a Gamemaster over Tor, please read the "How To Upgrade" section.

Note: In preparation for the enforcement, upgraded peers will start rejecting non-upgraded peers few hours before the enforcement block height, we recommend everyone to be updated at max a day before the final time.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).

**Important note for Gamemasters running over Tor (v2 onion address):**
Before starting the node, copy the content of the `onion_private_key` file, located inside the data directory into a new `onion_v3_private_key` file inside the same directory.
On linux: `cp -f onion_private_key onion_v3_private_key`.
If the `onion_v3_private_key` file already exist, replace the content with the content of the `onion_private_key` file.

This will allow you to bypass the v2 onion address deprecation and continue running the GM over Tor for now.
Be aware that the Tor network is **completely** removing v2 onion addresses support starting from Oct 15th (see "P2P and network changes" section).
After the v5.3 network upgrade enforcement, the GM will need to be migrated to run on a Tor v3 onion address (Update window Sept 15th - Oct 15th).
If it's not done on time, the node will drop off the network.

Sapling Parameters
==================

In order to run, as all versions after 5.0.0, hemis Core requires two files, `sapling-output.params` and `sapling-spend.params` (with total size ~50 MB), to be saved in a specific location.

For the following packages, no action is required by the user:
- macOS release `dmg` binaries will use the params that are bundled into the .app bundle.
- Windows installer `.exe` will automatically copy the files in the proper location.
- Linux `PPA/Snap` installs will automatically copy the files in the proper location.

For the other packages, the user must save the param files in the proper location:
- macOS/Linux `tar.gz` tarballs include a bash script (`install-params.sh`) to copy the parameters in the appropriate location.
- Windows `.zip` users need to manually copy the files from the `share/hemis` folder to the `%APPDATA%\hemisParams` directory.
- self compilers can run the script from the repository sources (`params/install-params.sh`), or copy the files directly from the `params` subdirectory.

Compatibility
==============

hemis Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.12+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

From hemis Core v5.3 onwards, macOS versions earlier than 10.12 are no longer supported.

hemis Core should also work on most other Unix-like systems but is not frequently tested on them.

The node's known peers are persisted to disk in a file called `peers.dat`. The format of this file has been changed in a backwards-incompatible way in order to accommodate the storage of Tor v3 and other BIP155 addresses. This means that if the file is modified by v5.3 or newer then older versions will not be able to read it. Those old versions, in the event of a downgrade, will log an error message "Incorrect keysize in addrman deserialization" and will continue normal operation as if the file was missing, creating a new empty one. ([PR #2411](https://github.com/hemis-Project/hemis/pull/2411))

Notable Changes
==============

GUI changes
-----------------------

The launch-on-startup option is no longer available on macOS

#### Subtract Fee From Amount Control

A new checkbox in the send widget allows the user to deduct the fee from the transaction recipient amount.
The most common use-case is when sending the whole balance, or a selection of UTXOs, without getting any change back.
If the transaction has multiple recipients, each recipient can be checked with a toggleable button in the context menu, and the fee will be split, and subtracted evenly from the outputs selected ([PR #2347](https://github.com/hemis-Project/hemis/pull/2347)).

#### Settings

A new checkbox added to the wallet settings to enable or disable automatic port mapping with NAT-PMP.
If both UPnP and NAT-PMP are enabled, a successful allocation from UPnP prevails over one from NAT-PMP.<br/>
Note: Successful automatic port mapping requires a router that supports either UPnP or NAT-PMP ([PR #2338](https://github.com/hemis-Project/hemis/pull/2338)).

#### RPC-Console

The GUI RPC-Console now accepts "parenthesized syntax", nested commands, and simple queries ([PR #2282](https://github.com/hemis-Project/hemis/pull/2282).
A new command `help-console` (available only on the GUI console) documents how to use it:

```
This console accepts RPC commands using the standard syntax.
    example:    getblockhash 0

This console can also accept RPC commands using parenthesized syntax.
    example:    getblockhash(0)

Commands may be nested when specified with the parenthesized syntax.
    example:    getblock(getblockhash(0) true)

A space or a comma can be used to delimit arguments for either syntax.
    example:    getblockhash 0
                getblockhash,0

Named results can be queried with a non-quoted key string in brackets.
    example:    getblock(getblockhash(0) true)[tx]

Results without keys can be queried using an integer in brackets.
    example:    getblock(getblockhash(0),true)[tx][0]
```

P2P and network changes
-----------------------

#### Tor v3 hidden services support - addrv2 message (BIP155)

This release adds support for Tor version 3 hidden services, and rumoring them over the network to other peers using [BIP155](https://github.com/bitcoin/bips/blob/master/bip-0155.mediawiki). Version 2 hidden services are still supported by hemis Core, but the Tor network has started [deprecating](https://blog.torproject.org/v2-deprecation-timeline) them, and will remove support soon. ([PR #2411](https://github.com/hemis-Project/hemis/pull/2411))

The Tor onion service that is automatically created by setting the `-listenonion` configuration parameter will now be created as a Tor v3 service instead of Tor v2. The private key that was used for Tor v2 (if any) will be left untouched in the `onion_private_key` file in the data directory (see `-datadir`) and can be removed if not needed. hemis Core will no longer attempt to read it. The private key for the Tor v3 service will be saved in a file named `onion_v3_private_key`. To use the deprecated Tor v2 service (not recommended), then `onion_private_key` can be copied over `onion_v3_private_key`, e.g.
`cp -f onion_private_key onion_v3_private_key`.

#### Removal of reject network messages from hemis Core (BIP61)

Nodes on the network can not generally be trusted to send valid ("reject") messages, so this should only ever be used when connected to a trusted node.
Please use the recommended alternatives if you rely on this feature:

* Testing or debugging of implementations of the hemis P2P network protocol should be done by inspecting the log messages that are produced by a recent version of hemis Core. hemis Core logs debug messages (`-debug=<category>`) to a stream (`-printtoconsole`) or to a file (`-debuglogfile=<debug.log>`).

* Testing the validity of a block can be achieved by specific RPCs (`submitblock`)

* Testing the validity of a transaction can be achieved by specific RPCs (`sendrawtransaction`)

* Wallets should not use the absence of "reject" messages to indicate a transaction has propagated the network, nor should wallets use "reject" messages to set transaction fees. Wallets should rather use fee estimation to determine transaction fees. Thus, they could wait until the transaction has confirmed (taking into account the fee target they set (compare the RPC `estimatesmartfee`) or listen for the transaction announcement by other network peers to check for propagation.

Multi-wallet support
--------------------

hemis Core now supports loading multiple, separate wallets ([PR #2337](https://github.com/hemis-Project/hemis/pull/2337)) with individual balances, keys and received transactions.

Multi-wallet is enabled by using more than one `-wallet` argument when starting hemis client, either on the command line or in the `hemis.conf` config file.

**In hemis-qt, only the first wallet will be displayed and accessible for creating and signing transactions.** GUI selectable multiple wallets will be supported in a future version. However, even in 5.3, other loaded wallets will remain synchronized to the node's current tip in the background.

hemis Core 5.3.0 contains the following changes to the RPC interface and hemis-cli for multi-wallet:

* When running hemis Core with a single wallet, there are **no** changes to the RPC interface or `hemis-cli`. All RPC calls and `hemis-cli` commands continue to work as before.
* When running hemis Core with multi-wallet, all *node-level* RPC methods continue to work as before. HTTP RPC requests should be send to the normal `<RPC IP address>:<RPC port>` endpoint, and `hemis-cli` commands should be run as before. A *node-level* RPC method is any method which does not require access to the wallet.
* When running hemis Core with multi-wallet, *wallet-level* RPC methods must specify the wallet for which they're intended in every request. HTTP RPC requests should be send to the `<RPC IP address>:<RPC port>/wallet/<wallet name>` endpoint, for example `127.0.0.1:8332/wallet/wallet1.dat`. `hemis-cli` commands should be run with a `-rpcwallet` option, for example `hemis-cli -rpcwallet=wallet1.dat getbalance`.

* A new *node-level* `listwallets` RPC method is added to display which wallets are currently loaded. The names returned by this method are the same as those used in the HTTP endpoint and for the `rpcwallet` argument.

The `getwalletinfo` RPC method returns the name of the wallet used for the query.

Note that while multi-wallet is now fully supported, the RPC multi-wallet interface should be considered unstable for version 5.3.0, and there may backwards-incompatible changes in future versions.

Reindexing changes
------------------

It is now possible to only redo validation, without rebuilding the block index, using the command line option `-reindex-chainstate` (in addition to `-reindex` which does both).
This new option is useful when the blocks on disk are assumed to be fine, but the chainstate is still corrupted. It is also useful for benchmarks ([PR #2391](https://github.com/hemis-Project/hemis/pull/2391)).

Mining/Staking transaction selection ("Child Pays For Parent")
--------------------------------------------------------------

The block-generation transaction selection algorithm now selects transactions based on their feerate inclusive of unconfirmed ancestor transactions.  This means that a low-fee transaction can become more likely to be selected if a high-fee transaction that spends its outputs is relayed.
With this change, the `-blockminsize` command line option has been removed.

Removal of Priority Estimation - Coin Age Priority
--------------------------------------------------

In previous versions of hemis Core, a portion of each block could be reserved for transactions based on the age and value of UTXOs they spent. This concept (Coin Age Priority) is a policy choice by miners/stakers, and there are no consensus rules around the inclusion of Coin Age Priority transactions in blocks.
hemis Core v5.3.0 removes all remaining support for Coin Age Priority ([PR #2378](https://github.com/hemis-Project/hemis/pull/2378)). This has the following implications:

- The concept of *free transactions* has been removed. High Coin Age Priority transactions would previously be allowed to be relayed even if they didn't attach a miner fee. This is no longer possible since there is no concept of Coin Age Priority. The `-limitfreerelay` and `-relaypriority` options which controlled relay of free transactions have therefore been removed.
- The `-blockprioritysize` option has been removed.
- The `prioritisetransaction` RPC no longer takes a `priority_delta` argument. The RPC is still used to change the apparent fee-rate of the transaction by using the `fee_delta` argument.
- `-minrelaytxfee` can now be set to 0. If `minrelaytxfee` is set, then fees smaller than `minrelaytxfee` (per kB) are rejected from relaying, mining and transaction creation. This defaults to 10000 satoshi/kB.
- The `-printpriority` option has been updated to only output the fee rate and hash of transactions included in a block by the mining code.

Configuration/Settings changes
------------------------------

#### Wallets directory configuration (`-walletdir`)

hemis Core now has more flexibility in where the wallets directory can be located. Previously wallet database files were stored at the top level of the hemis data directory ([PR #2423](https://github.com/hemis-Project/hemis/pull/2423)). The behavior is now:

- For new installations (where the data directory doesn't already exist), wallets will now be stored in a new `wallets/` subdirectory inside the data directory by default.

- For existing nodes (where the data directory already exists), wallets will be stored in the data directory root by default. If a `wallets/` subdirectory already exists in the data directory root, then wallets will be stored in the `wallets/` subdirectory by default.

- The location of the wallets directory can be overridden by specifying a `-walletdir=<path>` option where `<path>` can be an absolute path to a directory or directory symlink.

Care should be taken when choosing the wallets directory location, as if it becomes unavailable during operation, funds may be lost.

#### External wallet files

A new `-wallet=<path>` option allows the user to specify the wallet db location. It accepts either absolute paths or paths relative to the `-walletdir` directory. Multiple `-wallet` paths can be configured at the same time in order to run hemis Core with multiple wallets (see section "Multi-wallet support").

If `-wallet=<path>` is specified with a path that does not exist, it will create a wallet directory at the specified location (containing a `wallet.dat` data file, a `db.log` file, and `database/log.??????????` files).
If `-wallet=<path>` is specified with paths that are names of existing data files in the `-walletdir` directory, then log files are stored in the parent directory.

#### Configuration sections for testnet and regtest

It is now possible for a single configuration file to set different options for different networks ([PR #2324](https://github.com/hemis-Project/hemis/pull/2324)). This is done by using sections or by prefixing the option with the network, such as:
```
    main.uacomment=hemis
    test.uacomment=hemis-testnet
    regtest.uacomment=regtest
    [main]
    mempoolsize=300
    [test]
    mempoolsize=100
    [regtest]
    mempoolsize=20
```
The `addnode=`, `connect=`, `port=`, `bind=`, `rpcport=`, `rpcbind=`, and `wallet=` options will only apply to mainnet when specified in the configuration file, unless a network is specified.

#### Custom directory for blocks storage

A new initialization option flag `-blocksdir` allows the users to store block files outside of the data directory ([PR #2326](https://github.com/hemis-Project/hemis/pull/2326)).

#### Enable NAT-PMP port mapping at startup

The `-natpmp` option has been added to use NAT-PMP to map the listening port. If both UPnP and NAT-PMP are enabled, a successful allocation from UPnP prevails over one from NAT-PMP ([PR #2338](https://github.com/hemis-Project/hemis/pull/2338)).

#### asmap feature

A new `-asmap` configuration option has been added to diversify the node's network connections by mapping IP addresses Autonomous System Numbers (ASNs) and then limiting the number of connections made to any single ASN ([PR #2480](https://github.com/hemis-Project/hemis/pull/2480)).

#### Removed startup options

- `printstakemodifier`

Low-level RPC changes
---------------------

#### Support for JSON-RPC Named Arguments

Commands sent over the JSON-RPC interface and through the `hemis-cli` binary can now use named arguments. This follows the [JSON-RPC specification](http://www.jsonrpc.org/specification) for passing parameters by-name with an object.
`hemis-cli` has been updated to support this by parsing `name=value` arguments when the `-named` option is given. ([PR #2386](https://github.com/hemis-Project/hemis/pull/2386))

Some examples:

```
    src/hemis-cli -named help command="help"
    src/hemis-cli -named getblockhash height=0
    src/hemis-cli -named getblock blockhash=000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f
    src/hemis-cli -named sendtoaddress address="DMJRSsuU9zfyrvxVaAEFQqK4MxZg6vgeS6" amount="1.0" comment="donation"
```

The order of arguments doesn't matter in this case. Named arguments are also useful to leave out arguments that should stay at their default value.
The RPC server remains fully backwards compatible with positional arguments.

#### Query options for `listunspent` RPC

The `listunspent` RPC now takes a `query_options` argument ([PR #2317](https://github.com/hemis-Project/hemis/pull/2317)), which is a JSON object containing one or more of the following members:
- `minimumAmount` - a number specifying the minimum value of each UTXO
- `maximumAmount` - a number specifying the maximum value of each UTXO
- `maximumCount` - a number specifying the minimum number of UTXOs
- `minimumSumAmount` - a number specifying the minimum sum value of all UTXOs

The `listunspent` RPC also takes an additional boolean argument `include_unsafe` (true by default) to optionally exclude "unsafe" utxos.
An unconfirmed output from outside keys is considered unsafe ([PR #2351](https://github.com/hemis-Project/hemis/pull/2351)).

The `listunspent` output also shows whether the utxo is considered safe to spend or not.

#### Wallet name returned by `getwalletinfo`

`walletinfo` return object includes a `"walletname"` field. When hemis is not started with any `-wallet=<path>` options, the name of the default wallet returned by `getwalletinfo` (and `listwallets`) RPCs is the empty string `""`. If hemis is started with any `-wallet=<path>` options, the name of any wallet is its `<path>` string.

#### Removal of 'detach' argument from `stop`

The `stop` RPC no longer accepts the (already deprecated, ignored, and undocumented) optional boolean argument `detach`.

#### Subtract fee from recipient amount for RPC

- A new argument `subtract_fee_from` is added to `sendmany`/`shieldsendmany` RPC functions.
  It can be used to provide the list of recipent addresses paying the fee.
  ```
  subtract_fee_from         (array, optional)
  A json array with addresses.
  The fee will be equally deducted from the amount of each selected address.
    ["address"              (string) Subtract fee from this address
     ,...
    ]
  ```
  Examples:
  ```
  sendmany "" "{\"addr1\":10.04, \"addr2\":0.07, \"addr3\":100}" 1 "test" false "[\"addr1\", \"addr3\"]"

  shieldsendmany "from_shield" "[{\"address\": \"saddr1\", \"amount\":10.04, \"memo\": \"test\"}, {\"address\": \"saddr2\", \"amount\":0.07}, {\"address\": \"saddr3\", \"amount\":100}" 1 0 "[\"saddr1\", \"saddr3\"]"
  ```

- For `fundrawtransaction` a new key (`subtractFeeFromOutputs`) is added to the `options` argument.
  It can be used to specify a list of output indexes.
  ```
  subtractFeeFromOutputs    (array, optional)  A json array of integers.
  The fee will be equally deducted from the amount of each specified output.
  The outputs are specified by their zero-based index, before any change output is added.
    [vout_index,...]
  ```
  Example:
  ```
  fundrawtransaction "rawtxhex" "{\"subtractFeeFromOutputs\":[0,2]}"
  ```

- For `sendtoaddress`, the new parameter is called `subtract_fee` and it is a simple boolean.
  Example:
  ```
  sendtoaddress "addr1" 0.1 "test" "" true
  ```

In all cases the selected recipients will receive less HMS than their corresponding amount set.
If no outputs/addresses are specified, the sender pays the fee as usual. ([PR #2341](https://github.com/hemis-Project/hemis/pull/2341))

### New output fields in `getmempoolinfo`

`getmempoolinfo` return object now includes these additional fields:
```
"loaded": true|false      (boolean) Whether the mempool is fully loaded
"maxmempool": xxxxx       (numeric) Maximum memory usage for the mempool
"mempoolminfee": xxxxx    (numeric) Minimum fee rate in hemis/kB for tx to be
                          accepted. Is the maximum of minrelaytxfee and
                          minimum mempool fee
"minrelaytxfee": xxxxx    (numeric) Current minimum relay fee for transactions
```

#### `ischange` added to `getaddressinfo` output

Boolean value, to return whether the hemis (transparent) address was used for change output.

#### `mapped_as` added to `getpeerinfo` output

The `getpeerinfo` RPC now includes a mapped_as field to indicate the mapped Autonomous System used for diversifying peer selection. See the `-asmap` configuration option described above, in section "Configuration/Settings changes".

#### Show wallet's auto-combine settings in `getwalletinfo`

`getwalletinfo` now has two additional auto-combine related return fields. `autocombine_enabled` (boolean) and `autocombine_threshold` (numeric) that will show the auto-combine threshold and whether or not it is currently enabled ([PR #2248](https://github.com/hemis-Project/hemis/pull/2248)).

#### Deprecate the autocombine RPC command

The `autocombine` RPC command has been replaced ([PR #2248](https://github.com/hemis-Project/hemis/pull/2248)) with specific set/get commands (`setautocombinethreshold` and `getautocombinethreshold`, respectively) to bring this functionality further in-line with our RPC standards. Previously, the `autocombine` command gave no user-facing feedback when the setting was changed. This is now resolved with the introduction of the two new commands as detailed below:

* `setautocombinethreshold`
    ```
    setautocombinethreshold enable ( value )
    This will set the auto-combine threshold value.
    Wallet will automatically monitor for any coins with value below the threshold amount,
    and combine them if they reside with the same hemis address.  When auto-combine runs,
    it will create a transaction, and therefore will be subject to transaction fees.

    Arguments:
    1. enable          (boolean, required) Enable auto combine (true) or disable (false)
    2. threshold       (numeric, optional. required if enable is true) Threshold amount.
                       Must be greater than 1.

    Result:
    {
      "enabled": true|false,     (boolean) true if auto-combine is enabled, otherwise false
      "threshold": n.nnn,        (numeric) auto-combine threshold in HMS
      "saved": true|false        (boolean) true if setting was saved to the database, otherwise false
    }
    ```

* `getautocombinethreshold`
    ```
    getautocombinethreshold
    Returns the current threshold for auto combining UTXOs, if any

    Result:
    {
      "enabled": true|false,    (boolean) true if auto-combine is enabled, otherwise false
      "threshold": n.nnn        (numeric) the auto-combine threshold amount in HMS
    }
    ```

#### New RPC Commands

* `listwallets`
    ```
    Returns a list of currently loaded wallets.
    For full information on the wallet, use "getwalletinfo"
    Result:
    [                       (json array of strings)
      "walletname"            (string) the wallet name
       ...
    ]
    ```

* `rescanblockchain`
    ```
    rescanblockchain (start_height) (stop_height)
    Rescan the local blockchain for wallet related transactions.
    Arguments:
    1. start_height    (numeric, optional) block height where the rescan should start
    2. stop_height     (numeric, optional) the last block height that should be scanned
    Result:
    {
        start_height     (numeric) The block height where the rescan has started. If omitted,
                         rescan  started from the genesis block.
        stop_height      (numeric) The height of the last rescanned block. If omitted, rescan
                         stopped at the chain tip.
    }
    ```

* `getnodeaddresses`
    ```
    getnodeaddresses ( count "network" )

    Return known addresses which can potentially be used to find new nodes in the network

    Arguments:
    1. count        (numeric, optional) The maximum number of addresses to return. Specify 0
                       to return all known addresses.
    2. "network"  (string, optional) Return only addresses of the specified network. Can be
                       one of: "ipv4", "ipv6", "onion".
    Result:
    [
      {
        "time": ttt,          (numeric) Timestamp in seconds since epoch (Jan 1 1970 GMT)
                              when the node was last seen
        "services": n,        (numeric) The services offered by the node
        "address": "host",    (string) The address of the node
        "port": n,            (numeric) The port number of the node
        "network": "xxxx"     (string) The network (ipv4, ipv6, onion) the node connected through
      }
      ,...
    ]
    ```

* `importmulti`
    ```
    importmulti "requests" ( "options" )
    Import addresses/scripts (with private or public keys, redeem script (P2SH)), rescanning all addresses in one-shot-only (rescan can be disabled via options). Requires a new wallet backup.

    Arguments:
    1. requests     (array, required) Data to be imported
    [     (array of json objects)
      {
        "scriptPubKey": "script" | { "address":"address" }, (string / JSON, required) Type of
                                  scriptPubKey (string for script, json for address)
        "timestamp": timestamp | "now"                   (integer / string, required) Creation
                                  time of the key in seconds since epoch (Jan 1 1970 GMT), or the
                                  string "now" to substitute the current synced blockchain time.
                                  The timestamp of the oldest key will determine how far back
                                  blockchain rescans need to begin for missing wallet transactions.
                                  "now" can be specified to bypass scanning, for keys which are
                                  known to never have been used, and 0 can be specified to scan the
                                  entire blockchain. Blocks up to 2 hours before the earliest key
                                  creation time of all keys being imported by the importmulti call
                                  will be scanned.
        "redeemscript": "script",                        (string, optional) Allowed only if the
                                  scriptPubKey is a P2SH address or a P2SH scriptPubKey
        "pubkeys": ["pubKey", ... ],                     (array, optional) Array of strings giving
                                  pubkeys that must occur in the output or redeemscript
        "keys": ["key", ... ],                           (array, optional) Array of strings giving
                                  private keys whose corresponding public keys must occur in
                                  the output or redeemscript
        "internal": true|false,                          (boolean, optional, default: false) Stating
                                  whether matching outputs should be be treated as not incoming
                                  payments
        "watchonly": true|false,                         (boolean, optional, default: false) Stating
                                  whether matching outputs should be considered watched even when
                                  they're not spendable, only allowed if keys are empty
        "label": label,                                  (string, optional, default: '') Label to
                                  assign to the address, only allowed with internal=false
      }
    ,...
    ]
    2. options      (JSON, optional)
    {
       "rescan": true|false,         (boolean, optional, default: true) Stating if should rescan
                                     the blockchain after all imports
    }
    Result:
    [                                (Array) An array with the same size as the input that has the
                                     execution result
      {
        "success": true|false,    (boolean) True if import succeeded, otherwise false
        "error": {                (JSON Object) Object containing error information.
                                  Only present when import fails
          "code": n,              (numeric) The error code
          "message": xxxx         (string) The error message
        }
      }
      ,...
    ]
    Note: This call can take minutes to complete if rescan is true, during that time, other rpc calls
    may report that the imported keys, addresses or scripts exists but related transactions are still missing.
    ```

* `getmemoryinfo`
    ```
    Returns an object containing information about memory usage.
    Result:
    {
      "locked": {         (json object) Information about locked memory manager
        "used": xxxxx,        (numeric) Number of bytes used
        "free": xxxxx,        (numeric) Number of bytes available in current arenas
        "total": xxxxxxx,     (numeric) Total number of bytes managed
        "locked": xxxxxx,     (numeric) Amount of bytes that succeeded locking. If this number is
                              smaller than total, locking pages failed at some point and key data
                              could be swapped to disk.
        "chunks_used": xxxx,  (numeric) Number allocated chunks
        "chunks_free": xxxx,  (numeric) Number unused chunks
       }
    }
    ```
* `generatetoaddress`
    ```
    Mine blocks immediately to a specified address (before the RPC call returns)
    Arguments:
    1. nblocks          (numeric, required) How many blocks are generated immediately.
    2. "address"        (string, required) The address to send the newly generated HMSs to.
    Result:
    [ blockhashes ]     (array) hashes of blocks generated
    ```
  *Note*: this command is available only for RegTest

Database cache memory increased
--------------------------------

As a result of growth of the UTXO set, performance with the prior default database cache of 100 MiB has suffered.
For this reason the default was changed to 300 MiB in this release.
For nodes on low-memory systems, the database cache can be changed back to 100 MiB (or to another value) by either:
- Adding `dbcache=100` in `hemis.conf`
- Adding `-dbcache=100` startup flag
- Changing it in the GUI under `Settings → Options → Main → Size of database cache`

Note that the database cache setting has the most performance impact during initial sync of a node, and when catching up after downtime ([PR #2391](https://github.com/hemis-Project/hemis/pull/2391)).

Updated settings
----------------

Netmasks that contain 1-bits after 0-bits (the 1-bits are not contiguous on the left side, e.g. 255.0.255.255) are no longer accepted. They are invalid according to RFC 4632.

Build system changes
--------------------

#### C++14 standard

hemis Core now requires a C++14 compiler ([PR #2269](https://github.com/hemis-Project/hemis/pull/2269)).

#### Bump miniUPnPc API version

The minimum supported miniUPnPc API version is set to 10. This keeps compatibility with Ubuntu 16.04 LTS and Debian 8 `libminiupnpc-dev` packages. Please note, on Debian this package is still vulnerable to [CVE-2017-8798](https://security-tracker.debian.org/tracker/CVE-2017-8798) (in jessie only) and [CVE-2017-1000494](https://security-tracker.debian.org/tracker/CVE-2017-1000494) (both in jessie and in stretch).

OpenSSL is no longer used by hemis Core

#### Disabled PoW mining RPC Commands

A new configure flag has been introduced to allow more granular control over weather or not the PoW mining RPC commands are compiled into the wallet. By default they are not. This behavior can be overridden by passing `--enable-mining-rpc` to the `configure` script ([PR #2105](https://github.com/hemis-Project/hemis/pull/2105)).

#### NAT-PMP Support

hemis v5.3.0 added NAT-PMP port-mapping support via [`libnatpmp`](https://miniupnp.tuxfamily.org/libnatpmp.html). Similar to how UPnP is treated, NAT-PMP will be compiled if the library is found, but the functionality is disabled unless either using the `-natpmp` option at startup (see section "Configuration changes"), or selecting the relative checkbox in the graphical interface (see section "GUI changes").

Alternatively, hemis Core can be compiled with the functionality enabled by default passing `--enable-natpmp-default` to the `configure` script.

ISO 8601 for timestamps in logs and backup file names
-----------------------------------------------------

The log timestamp format is now ISO 8601 (e.g. "2021-02-28T12:34:56Z").
The file extension applied to automatic backups is now in ISO 8601 basic notation (e.g. "20210228T123456Z"). The basic notation is used to prevent illegal `:` characters from appearing in the filename ([PR #2300](https://github.com/hemis-Project/hemis/pull/2300)).


v5.3.0 Change log
==============

### Consensus
- #2269 [Build] v6 Network upgrade - C++14 standard (random-zebra)
- #2274 [Refactoring] Blocks v10: TierTwo payments in the coinbase transaction (random-zebra)
- #2275 New cold-staking opcode (random-zebra)
- #2492 v5.3 network upgrade (furszy)
- #2518 [Consensus] Establish v5.3 NU enforcement height for mainnet (furszy)

### Block and transaction handling
- #2284 Removing unneeded block serialization and signal from ABC (furszy)
- #2290 [Test] Fix concurrency issues in ActivateBestChain + optimization (furszy)
- #2268 [Core] Store disconnected blocks txes outside mempool during reorg (random-zebra)
- #2251 Crash if disconnecting a block fails (furszy)
- #2295 [Net_Processing] Cleaner block processing workflow (furszy)
- #2328 Always flush block and undo when switching to new file (furszy)
- #2339 [Bugfix] the end of a reorged chain is invalid when connect fails (furszy)
- #2336 Refactor block file logic + util.h/cpp moved to util/system.h (furszy)
- #2345 [TierTwo] Gamemaster collateral auto-locking + more tests (random-zebra)
- #2349 [RPC] Provider-Update-Service special tx type (random-zebra)
- #2363 [RPC] ProUpReg and ProUpRev special tx types (random-zebra)
- #2378 Child-pays-for-parent Implementation + CoinAge priority removal (random-zebra)
- #2434 Decouple enforcement of cbase payment from block version (random-zebra)
- #2464 Decouple peer-processing-logic from block-connection-logic + BIP61 removal (furszy)
- #2484 [BUG] Zerocoin: fix spend validation and remove obsolete sporks (random-zebra)
- #2404 [Refactoring] Improve pos checks in AcceptBlock (random-zebra)

### P2P protocol and network code
- #2257 [Backport] Ensure tried collisions resolve, and allow feeler connections to existing outbound netgroups (furszy)
- #2330 Drop support of the insecure miniUPnPc versions (Fuzzbawls)
- #2361 [Bug] Fix expected disconnections after not-connectable block (random-zebra)
- #2338 Add NAT-PMP port forwarding support (Fuzzbawls)
- #2417 [net processing] Swap out signals for an interface class (furszy)
- #2422 Mockable PoissonNextSend helper and tests mempool sync fixes (furszy)
- #2480 asmap to improve IP bucketing in addrman - backports (furszy)
- #2411 Road to Tor v3 support (BIP155) (furszy)
- #2510 [TierTwo] Enforce MessageVersion::MESS_VER_HASH (gmb/gmp) with 5.3 (random-zebra)
- #2502 Add Tor v3 hardcoded seeds (Fuzzbawls)
- #2508 [BugFix] Correct gmb ser/unser of BIP155 node addresses (furszy)

### Wallet
- #2240 Add `rescanblockchain` RPC command functionality (furszy)
- #2276 [Core] Locked memory manager (random-zebra)
- #2281 Improve rescan API + avoid permanent locks during RescanFromTime (random-zebra)
- #2241 Conflicted transactions external listeners notification fix (furszy)
- #2299 Remove now unneeded stake P2PKH->P2PK output forced conversion (furszy)
- #2362 [Refactoring] Move automatic-wallet-backup code out of init.cpp (random-zebra)
- #2337 Basic multiwallet support (random-zebra)
- #2398 Kill AvailableCoinsType enum (furszy)
- #2351 Laggard wallet-related backports from btc 0.15 (random-zebra)
- #2341 [RPC] Add subtract-fee-from-amount option in Create[Fund]Transaction (random-zebra)
- #2369 Clean wallet auto-backup "feature" (furszy)
- #2423 Introduce wallets directory configuration and external wallet files capabilities (furszy)
- #2483 [BUG][GUI] Fix broken tutorial screen (random-zebra)

### RPC and other APIs
- #2277 Add `importmulti` RPC command and functional test (random-zebra)
- #2283 `getwalletinfo`: Add last_processed_block return value (furszy)
- #2302 Return command name with 'Method not found' errors (random-zebra)
- #2296 Provider-Register transactions (random-zebra)
- #2248 refactor autocombine into specific commands (Fuzzbawls)
- #2297 Implement generatetoaddress RPC command (furszy)
- #2317 Add query options to listunspent rpc call (furszy)
- #2375 [RPC] Update getrawtransaction warning message (random-zebra)
- #2386 Support for JSON-RPC named arguments (random-zebra)
- #2392 [RPC] Remove compiler warning in autocombinerewards and update help text (random-zebra)
- #2476 correct non hemisified help commands texts and add missing "ischange" result (furszy)
- #2503 [Net] Add getnodeaddresses RPC command (Fuzzbawls)
- #2519 [RPC] Do not register DGM-related RPC commands (random-zebra)

### GUI
- #2249 Make box of hemis Send address return to purple when it's empty (PicklesRcool)
- #2247 Fix double fade-in Topbar Available click animation. Issue 2216 (PicklesRcool)
- #2237 Set fee to highest possible if input is too big (dnchk)
- #2264 fix 'split is deprecated' warnings (Fuzzbawls)
- #2282 RPC-Console support nested commands and simple value queries (random-zebra)
- #2215 Automatically set the lowest possible Custom Fee when user provided fee is too low (MishaPozdnikin)
- #2306 if the custom fee is disabled, use minimum required fee (furszy)
- #2293 [Refactoring] Constrain direct wallet access from the GUI in a single place (random-zebra)
- #2286 Remove BIP70 (Fuzzbawls)
- #2321 Fix Cold Staking address list (Fuzzbawls)
- #2327 [BUG] Fix crash when sending shield notes from the GUI with coin control (random-zebra)
- #2372 RPC-Console nested commands documentation (random-zebra)
- #2388 Ensure that all UI error/warning texts pass through translation (Fuzzbawls)
- #2403 transaction model, cleanup unused hasZcTxes flag (furszy)
- #2397 Fix invalid GM collateral amount and align every static 10k HMS strings (furszy)
- #2380 [Cleanup] guiutils cleanup + fix memory mismanagement (furszy)
- #2347 Add GUI controls for subtract-fee-from-amount (random-zebra)
- #2490 [BugFix] fix app always triggering the tutorial dialog if no manual '-wallet' arg is provided (furszy)
- #2448 Periodic make translate (Fuzzbawls)
- #2481 [BUG][GUI] Fix P2CS grouping in coin control (random-zebra)
- #2506 bugfix, the command line interface is called "hemis-cli" not "hemis-core" (furszy)
- #2513 [GUI] Remove extra jump line in gmb creation error message (furszy)
- #2516 [GUI][BUG] Console: allow empty arguments (random-zebra)

### Platform support
- #2449 Solve filename and command line encoding issues on Windows (furszy)

### Tier two network
- #2271 [DB] Introduce EVO database (random-zebra)
- #2273 [TierTwo] Deterministic gamemaster lists (random-zebra)
- #2309 New gamemaster payment logic (random-zebra)
- #2308 Compatibility code for GM payments + budget voting (random-zebra)
- #2322 Move evodb_cache Memory Usage logging (PeterL73)
- #2402 Fix GM activation when the node received the gmb before initialize the GM (furszy)
- #2501 [BUG][RPC] Fix listgamemasters and getgamemasterwinners (random-zebra)
- #2514 [TierTwo][BugFix] Fix peer banning for not synced tiertwo + GetGMList and inv performance improvements (furszy)

### Build system
- #2263 Prune un-necessary X packages (Fuzzbawls)
- #2280 [Depends] Cleanup Rust and add multi-arch linux support (Fuzzbawls)
- #2291 Remove un-used variable (Fuzzbawls)
- #2307 Don't explicitly include generated moc file (Fuzzbawls)
- #2288 [Cleanup] Remove deprecated OpenSSL Bignum support (Fuzzbawls)
- #2342 Bump master to 5.1.99 (Fuzzbawls)
- #2332 [CMake] Add hints path for user-installed cargo bindir (Fuzzbawls)
- #2329 Bump miniupnpc to 2.2.2 (Fuzzbawls)
- #2352 Remove OpenSSL (Fuzzbawls)
- #2357 Allow export of environ symbols (furszy)
- #2370 [GA] Workaround missing apt dependency (Fuzzbawls)
- #2331 Bump boost to 1.71.0 and optimize flow (Fuzzbawls)
- #2389 Clean coverage files from budget and evo paths (Fuzzbawls)
- #2272 Update macOS cross-compiling toolchain (Fuzzbawls)
- #2400 Unify macOS deploy target (Fuzzbawls)
- #2418 [GA] Add native macOS build job (Fuzzbawls)
- #2461 Bump master to 5.2.99 and save historical release notes (random-zebra)
- #2465 fix macOS depends deployment (furszy)
- #2471 fix GA boost test random seed number overflow (furszy)
- #2426 Update crc32c subtree (Fuzzbawls)
- #2447 [Cleanup] Tidy up the gitian build descriptors (Fuzzbawls)
- #2479 bench build, raw generated file improvement and fixes (furszy)
- #2419 Add chiabls subtree (Fuzzbawls)
- #2520 [Gitian] Set version to 5.3 (Fuzzbawls)

### Tests and QA
- #2254 sync_blocks and sync_mempool improvements (random-zebra)
- #2285 Eliminate race condition in mempool_packages test (furszy)
- #2279 Fix wallet_import_rescan (random-zebra)
- #2313 Expand dbwrapper unit test coverage (furszy)
- #2324 [Core] Network specific configuration sections (random-zebra)
- #2340 [BugFix] Fixing sapling_wallet.py getshieldbalance race condition (furszy)
- #2326 [Misc] Fix libsecp256k1 init for unit test setup + Allow to specify blocks storage directory (furszy)
- #2346 Test suite update + P2P invalid blocks and invalid tx functional tests coverage (furszy)
- #2365 [BugFix] Fix failing p2p_invalid_block.py (furszy)
- #2364 Coverage for zc transactions rejection in mempool and blocks (furszy)
- #2366 validation_block_tests: Correcting BOOST_ASSERT_MSG usage for dynamic messages (furszy)
- #2367 [BUG][Tests] mining_pos_* tests calling `generate()` or `generate_pow()` (random-zebra)
- #2356 [Refactoring] Remove mocked budget manager in budget_tests (random-zebra)
- #2383 [BUG][QA] Use generate_pos() after POW phase end in mining_pos_fakestake (random-zebra)
- #2360 Resurrect feature_block and kill the p2p comparison test framework (random-zebra)
- #2393 [BUG][QA] Don't switch to POS in feature_fee_estimation (random-zebra)
- #2390 [BUG][Tests] Properly set temporary datadir for dbwrapper_basic_data (random-zebra)
- #2252 Fuzzing framework support (furszy)
- #2436 Add tiertwo_governance_reorg functional test (random-zebra)
- #2427 Add test coverage for invalid block budget payee script (furszy)
- #2410 Add p2p invalid messages functional test (and test framework update) (furszy)
- #2414 Finish address encoding cleanup (furszy)
- #2444 Fix wallet_encryption.py race condition (furszy)
- #2452 Clean and lint the python regtest suite (Fuzzbawls)
- #2438 Extended test coverage for chain reorg during superblock and payments order (furszy)
- #2485 fixing python lint, W292 no newline at end of file (furszy)
- #2486 Fix ProcessNewBlock signature in budget_tests.cpp (random-zebra)
- #2487 [BUG][Tests] Delay POS activation in feature_blockhashcache test (random-zebra)
- #2489 Require exact match in assert_raises_init_error (random-zebra)

### Miscellaneous
- #2246 util: Pass pthread_self() to pthread_setschedparam instead of 0 (Fuzzbawls)
- #2289 [Core] Fix several potential issues found by sanitizers (random-zebra)
- #2256 [Backport] Serialization updates (furszy)
- #2278 Update RNG code from upstream (Fuzzbawls)
- #2294 [Sapling][Refactoring] Save wallet reference inside SaplingOperation (random-zebra)
- #2301 [net_processing] Improve/upgrade orphan transactions handling (furszy)
- #2312 Increase LevelDB max_open_files unless on 32-bit Unix (furszy)
- #2304 Addrdb code deduplication + Native support for FLATDATA + Add file syncing logging and error handling (furszy)
- #2343 util: Remove boost program_options dependency (Fuzzbawls)
- #2300 util: Time utility updates (Fuzzbawls)
- #2311 [Cleanup] Remove zc PrivateCoin and Accumulator classes (random-zebra)
- #2319 [Cleanup] Remove obsolete --printstakemodifier startup option (random-zebra)
- #2320 [Cleanup] Remove zerocoin mints databasing (random-zebra)
- #2368 [Rand] Use thread-safe atomic in perfmon seeder (Fuzzbawls)
- #2314 [Refactoring] Completed migration of uint* classes to blob (random-zebra)
- #2376 Replace deprecated throw() with noexcept specifier (C++11) (random-zebra)
- #2359 Net code updates [Step 2] (furszy)
- #2391 [Core] Add reindex-chainstate startup option (random-zebra)
- #2459 Fix install-params.sh "popd: directory stack empty" (furszy)
- #2412 [Backport] Serialization framework updates (furszy)
- #2460 Reorder C{,Mutable}Transaction for better packing (random-zebra)
- #2470 Span sources updates, up-to-date with current upstream (furszy)
- #2473 [Backport] git-subtree-check.sh updated to latest upstream (furszy)
- #2462 [Refactoring] Bitcoin 0.16 cherries (random-zebra)
- #2495 [Core] Update LevelDB to latest upstream (Fuzzbawls)
- #2497 [Bug] Fix improper locking on Windows WSL1 environments (Fuzzbawls)
- #2496 scripts: Ensure utf8 encoding for ascii file operations in python scripts (Fuzzbawls)

### Documentation
- #2262 add reduce-memory.md (furszy)
- #2344 Save historical release notes for v5.1.0 (random-zebra)
- #2466 Remove OpenSSL instructions in build-osx.md (Fuzzbawls)
- #2267 Introducing Deterministic Gamemasters (random-zebra)
- #2500 Updated documentation related to macOS SDK extraction (random-zebra)

### Refactoring
- #2253 Wallet db keys enum + old CWalletKey removal (furszy)
- #2270 Mix refactoring in preparation for DGM lists (random-zebra)
- #2325 fix indenting and code styling in netaddress.h/.cpp (random-zebra)
- #2399 One CBaseChainParams should be enough (Fuzzbawls)
- #2373 Combine scriptPubKey and amount as CTxOut in CScriptCheck (random-zebra)
- #2407 [Cleanup] Housekeeping: remove unneeded CLegacyBlockIndex, proof.h/cpp and txdb zc functions (furszy)
- #2425 [Cleanup] The zerocoin garbage collector is in town once more (furszy)
- #2478 [Cleanup] remove g_newP2CSRules flag (random-zebra)
- #2468 remove std::string DecodeBase58(const char*, int) (random-zebra)
- #2467 Replace MakeUnique with std::make_unique (C++14) (random-zebra)
- #2488 [Doc] Add missing codeblock triple backtick in relnotes file (random-zebra)
- #2453 Create generic CallResult and connect it to getNewAddress* functions (furszy)
- #2504 Clang-tidy and fix compiler error in HasReason class (Fuzzbawls)
- #2507 [Cleanup] Drop unneeded protocol version checks and constants (random-zebra)
- #2482 Make HexStr take a span (furszy)
- #2505 [Cleanup][Trivial] Remove unused SPORK_5 (random-zebra)
- #2511 [GUI] Clean up gminfodialog (Fuzzbawls)

## Credits

Thanks to everyone who directly contributed to this release:

- Aaron Clauson
- Alex Morcos
- Alexander Block
- Alin Rus
- Amiti Uttarwar
- Andrew Chow
- Anthony Towns
- Antoine Riard
- Ben Woosley
- Carl Dong
- chris-belcher
- Chun Kuan Lee
- Cory Fields
- Dan Raviv
- Daniel Kraft
- Daniel McNally
- dnchk
- donaloconnor
- Elichai Turkel
- Eric Lombrozo
- Evan Klitzke
- Fabian Jahr
- fanquake
- furszy
- Fuzzbawls
- Gleb Naumenko
- Gregory Maxwell
- Gregory Sanders
- Hennadii Stepanov
- James O'Beirne
- Jarol Rodriguez
- Jeremy Rubin
- Jesse Cohen
- Jim Posen
- jjz
- John Newbery
- Johnson Lau
- Jon Atack
- Jonas Schnelli
- João Barbosa
- kallewoof
- Karl-Johan Alm
- Kaz Wesley
- kazcw
- ken2812221
- Kittywhiskers Van Gogh
- Kristaps Kaupe
- Larry Ruane
- Lenny Maiorani
- lucash-dev
- Luke Dashjr
- Luke Mlsna
- MapleLaker
- marcaiaf
- Marco
- marcoagner
- MarcoFalke
- Mark Tyneway
- Marko Bencun
- Matt Corallo
- MeshCollider
- MishaPozdnikin
- murrayn
- NicolasDorier
- Patrick Strateman
- Pavel Janík
- Peter Bushnell
- PeterL73
- Pieter Wuille
- practicalswift
- pstratem
- qmma
- random-zebra
- René Nyffenegger
- Russell Yanofsky
- Ryan Havar
- Samer Afach
- Samuel Dobson
- Sebastian Falbesoner
- Sjors Provoost
- skmcontrib
- Suhas Daftuar
- Troy Giorshev
- Vasil Dimov
- Volodia
- W. J. van der Laan
- wh
- winder
- Wladimir J. van der Laan
- wodry

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.