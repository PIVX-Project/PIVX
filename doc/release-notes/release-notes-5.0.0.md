hemis Core version *v5.0.0* is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>


Mandatory Update
==============

hemis Core v5.0.0 is a mandatory update for all users. This release contains a protocol upgrade (to `70920`), new consensus rules and improvements that are not backwards compatible with older versions. Users will need to update their clients before enforcement of this update goes into effect.

Update enforcement is currently scheduled to go into effect at the following block height:
```
Mainnet: 2,700,500
```
Gamemasters will need to be restarted once both the gamemaster daemon and the controller wallet have been upgraded.

Note: In preparation for the enforcement, upgraded peers will start rejecting non-upgraded peers few hours before the enforcement block height, we recommend everyone to be updated at max a day before the final time.

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).

#### Sapling Parameters
In order to run, hemis Core now requires two files, `sapling-output.params` and `sapling-spend.params` (with total size ~50 MB), to be saved in a specific location.

For the following packages, no action is required by the user:
- macOS release `dmg` binaries will use the params that are bundled into the .app bundle.
- Windows installer `.exe` will automatically copy the files in the proper location.
- Linux `PPA/Snap/Copr` installs will automatically copy the files in the proper location.

For the other packages, the user must save the param files in the proper location, before being able to run hemis v5.0.0:
- macOS/Linux `tar.gz` tarballs include a bash script (`install-params.sh`) to copy the parameters in the appropriate location.
- Windows `.zip` users need to manually copy the files from the `share/hemis` folder to the `%APPDATA%\hemisParams` directory.
- self compilers can run the script from the repository sources (`params/install-params.sh`), or copy the files directly from the `params` subdirectory.



Compatibility
==============

hemis Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.10+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

Apple released it's last Mountain Lion update August 13, 2015, and officially ended support on [December 14, 2015](http://news.fnal.gov/2015/10/mac-os-x-mountain-lion-10-8-end-of-life-december-14/). hemis Core software starting with v3.2.0 will no longer run on MacOS versions prior to Yosemite (10.10). Please do not report issues about MacOS versions prior to Yosemite to the issue tracker.

hemis Core should also work on most other Unix-like systems but is not frequently tested on them.


Notable Changes
==============

New SHIELD Protocol Implemented!
--------------------------------

#### Overview
Users will be able to protect their financial information by sending and receiving HMSs privately, as well as sending and receiving encrypted messages attached to the shield transactions.
More visual information about the protocol can be found at https://hemis.org .

#### GUI features
New set of functionalities for the interaction with the SHIELD protocol:

* Receive screen modified to show, generate and set label of wallet's shield addresses.
* Dashboard transactions list including shield transactions.
* Top bar showing shield HMS balances.
* Send screen modified, implementing shield transaction crafting and broadcast.
* New encrypted memo functionality.
* Transaction detail dialog presenting shield transaction information.
* Spending process moved to a background thread, loading screen connected while the operation is being performed.
* Contacts screen modified to store external shield addresses.

#### Wallet
A brand new manager encapsulating all Sapling related capabilities inside the wallet has been implemented:

* New address type: shield addresses (using bech32 format).
* New derivation path for Sapling keys: Shield addresses are derived from the same wallet master seed used for the deterministic derivation of all other keys. The same seed, therefore, can be used to restore both transparent and Shield addresses, recovering both transparent and Shield HMS balance.
* Support for Sapling extended full viewing keys, incoming viewing keys, outgoing viewing keys and spending keys.
* Sapling notes management:
    - Notes decryption.
    - Blockchain scanning protocol: handling when and how to store a Sapling output and its related keys. As well as mark used notes.
    - Notes metadata cache: retrieving information without requiring to decrypt notes on-demand.
    - Able to filter available notes for spending.
* Sapling witnesses and nullifiers tracking.
* New transaction building process, crafting and signing shield transactions.

#### Block Primitive:
* Block version bumped to 8.
* Starting from block v8, the block header includes an additional `uint256` field, `hashFinalSaplingRoot` (which adds 32 bytes to the end of the serialized block header) storing the root of the Sapling note commitment tree, corresponding to the final Sapling treestate of this block.
* Each block index now is tracking the network total value entering and exiting the shield pool.

#### Transaction Primitive:
* The "transaction type" concept was introduced, dividing the version field (4 bytes) in version (first 2 bytes) and type (second 2 bytes).
* For transactions with `nVersion >= 3`, a new optional field `sapData` has been included, comprising:
    - `valueBalance`: the net value of Sapling spend transfers minus output transfers
    - `vShieldedSpend`: a sequence of Sapling spend descriptions
    - `vShieldedOutput`: a sequence of Sapling output descriptions
    - `bindingSig`: the Sapling binding signature
* Transactions with `nVersion >= 3` and `nType != 0`(not enabled by consensus at the moment), are defined "special transactions" and must include an extra payload serialized according to the definition imposed by the relative type.
* A new signature hash for Sapling and Special transactions has been implemented (as defined in [ZIP-243](https://zips.z.cash/zip-0243)).

#### Additional Consensus Rules
- Transactions with version >= 3 must have the new serialization (including a zero byte for empty `Optional`, if missing `sapData` or `extraPayload`).
- Shield transactions cannot be coinbase or coinstake transactions.
- Transactions containing empty `vin` must have non-empty `vShieldedSpend`.
- Transactions containing empty `vout` must have non-empty `vShieldedOutput`.
- Transactions with version >= 4 are not accepted
- The maximum size of a single Shield transaction is 400 kB.
- The sum of the sizes of all Shield transactions in a block cannot be greater than 750 kB.

#### Fee Policy
The `minRelayTxFee` startup argument is multiplied by a fixed factor `K = 100` for Shield transactions. This makes the fee required to relay Shield transactions (and "dust" threshold value) 100 times higher than a transparent transaction of the same size.

#### Build System
In order to support the protocol, the following dependencies are introduced in the build system:
- `Sodium` https://github.com/jedisct1/libsodium
- `Rust/Cargo`https://www.rust-lang.org/

HD Wallet upgrade
-----------------
In order to support the Shield features, the wallet must be upgraded to `SAPLING_VERSION`.

*How do I check my wallet version?*
* Check the output of `getinfo` or `getwalletinfo`.<br>The `"walletversion"` field must be equal to `170000` (`SAPLING_VERSION`)

*How do I upgrade my HD wallet to `SAPLING_VERSION`?*
* Newly created wallets have already the latest version
* If the wallet is unlocked, use the `-upgradewallet` init flag to automatically update at startup.<br>Otherwise use the GUI or RPC (see below) to unlock and upgrade the wallet.
* GUI:
    - A dialog will appear on every wallet startup notifying you that you are running a pre-Sapling wallet and letting you upgrade it from there.
    - If you haven't upgraded your wallet, the topbar (bar with icons that appears at the top of your wallet) will have an "HD" icon. Click it and the upgrade dialog will be launched.
* RPC:
    - use the `upgradewallet` after unlocking the wallet (if locked)

Tier Two Network
----------------
A large number of performance and stability improvements over the complete tier two network and gamemasters sources have been performed. Re-writing, encapsulating and decoupling workflows, cleaning up an extensive number of redundancies and misalignment.
Plus, tier two and gamemasters capabilities have been introduced to regtest, enabling the local testing environment setup and the creation of a functional testing framework for the area.
This work is directly correlated with a substantial efficiency improvement for the entire software, not only for gamemasters, and is one of the building blocks for the new tier two network and sync protocol that will be continued post-v5.

Instant Proposal Removal:
The threshold was increased to 30% of negative votes.
The proposal will never be removed before its first superblock.

Maximum Proposal Payment:
The limit was decreased to a maximum of 6 payments.

Concurrency
-----------
A complete overhaul of the thread synchronization area has been done, all of the workflows that were containing locks with cyclic dependencies that could cause a deadlock were reworked.
Solving every circular dependency, a good number of inconsistent states were fixed. The software now can be fully run with the `--enable-debug` configure time flag without crashing for a possible deadlock.

Account API Removed
-------------------

The 'account' API was deprecated in v4.2.0, and has been fully removed in v5.0.0.
The 'label' API was introduced in v4.2.0 as a replacement for accounts.

See the release notes from [v4.2.0](https://github.com/hemis-Project/hemis/blob/master/doc/release-notes/release-notes-4.2.0.md#label-and-account-apis-for-wallet) for a full description of the changes from the 'account' API to the 'label' API.

SwiftX Removed
--------------
Remove obsolete functionality (already disabled via SPORK in the previous releases).
- startup arguments removed:
    * `zmqpubhashtxlock`
    * `zmqpubrawtxlock`
    * `enableswifttx`
    * `swifttxdepth`


- P2P messages deprecated:
    * `MSG_TXLOCK_REQUEST`
    * `MSG_TXLOCK_VOTE`


- GUI changes:
    * remove SwiftX mentions in coin control


- RPC commands removed/modified:
    * `sendtoaddressix` (removed)
    * `sendrawtransaction` (removed `swiftx` optional parameter)
    * decoded transaction `bcconfirmations` JSON entry is now deprecated and kept only for backward compatibility (it is equal to `confirmations`)


- Deprecated Sporks:
    * `SPORK_2_SWIFTTX`
    * `SPORK_3_SWIFTTX_BLOCK_FILTERING`

Zerocoin Protocol Support Removed
---------------------------------
The Core wallet is no longer able to produce (public) zerocoin spends, and those are no longer accepted by consensus.
- Several zerocoin related RPC commands have been removed or changed (as  detailed in the section "RPC Changes" below).
- startup argument removed: `reindexzerocoin`
- GUI: removed transaction records for zerocoin mint outputs


GUI Changes
-----------
#### Shield Protocol
Several views and widgets have been updated to support new features as described in the previous section.

#### RPC Console
- Added autocompletion for the `help` command
- Added warning/confirmation dialog before executing commands that expose private keys (`dumpwallet`, `dumpprivkey`, `exportsaplingkey`)


RPC Changes
------------------
The RPC server now returns a JSON-RPC exception when receiving a request during the warm-up phase. The returned exception includes the current init message ("Loading sporks", "Loading block index", "Verifying blocks", etc...)

#### New RPC Commands

Several new RPC commands have been introduced to support SHIELD address and transaction interactions, as well as expanded informational and layer 2 functionality. The below table lists each command and it's intended purpose.

| Command Name | Purpose | Requires Unlocked Wallet? |
| ------------ | ------- | ------------------------- |
| `getbestsaplinganchor` | Returns the most recent SaplingMerkleTree root | No |
| `exportsaplingkey` | Exports the private key of a SHIELD address | Yes |
| `exportsaplingviewingkey`| Exports the viewing key of a SHIELD address | Yes |
| `getnewshieldaddress` | Creates a new SHIELD address | Yes |
| `getsaplingnotescount` | Returns the number of sapling notes available in the wallet | No |
| `getshieldbalance` | Return information about the shield value of funds stored in the wallet | No |
| `importsaplingkey` | Imports the private key of a SHIELD address | Yes |
| `importsaplingviewingkey` | Imports the viewing key of a SHIELD address | Yes |
| `listreceivedbyshieldaddress` | Returns a list of amounts received by a SHIELD address in the wallet | No |
| `listshieldaddresses` | Returns the list of shield addresses belonging to the wallet | No |
| `listshieldunspent` | Returns array of unspent SHIELD notes in the wallet | No |
| `rawshieldsendmany` | Creates a transaction sending to many recipients (without committing it), and returns the hex string. | Yes |
| `shieldsendmany` | Send to many recipients (Either transparent or SHIELD) | Yes |
| `viewshieldtransaction` | Get detailed SHIELD information about an in-wallet transaction | Yes |
| `getsupplyinfo` | Returns detailed HMS supply information | No |
| `initgamemaster` | Manually initialize the client as a gamemaster | No |
| `getcachedblockhashes` | Return the block hashes cached in the gamemaster manager | No |

Each new command is detailed below:

* `getbestsaplinganchor`
  ```
  Returns the most recent SaplingMerkleTree root.

  Result
  "hex"      (string) the sapling anchor hex encoded
  ```
* `exportsaplingkey`
  ```
  Reveals the key corresponding to the 'shield_addr'.
  Then the importsaplingkey can be used with this output

  Arguments:
  1. "addr"   (string, required) The shield addr for the private key

  Result:
  "key"       (string) The private key
  ```
* `exportsaplingviewingkey`
  ```
  Reveals the viewing key corresponding to 'shield_addr'.
  Then the importsaplingviewingkey can be used with this output

  Arguments:
  1. "shield_addr"   (string, required) The shield addr for the viewing key

  Result:
  "vkey"               (string) The viewing key
  ```
* `getnewshieldaddress`
  ```
  Returns a new shield address for receiving payments.

  Result:
  "address"    (string) The new shield address.
  ```
* `getsaplingnotescount`
  ```
  Returns the number of sapling notes available in the wallet.

  Arguments:
  1. minconf      (numeric, optional, default=1) Only include notes in transactions confirmed at least this many times.

  Result:
  num             (numeric) the number of sapling notes in the wallet
  ```
* `getshieldbalance`
  ```
  Return the total shield value of funds stored in the node's wallet or if an address was given,
  returns the balance of the shield addr belonging to the node's wallet.

  CAUTION: If the wallet contains any addresses for which it only has incoming viewing keys,
  the returned private balance may be larger than the actual balance, because spends cannot
  be detected with incoming viewing keys.

  Arguments:
  1. "address"        (string, optional) The selected address. If non empty nor "*", it must be a Sapling address
  2. minconf          (numeric, optional, default=1) Only include private and transparent transactions confirmed at least this many times.
  3. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress' and 'importsaplingviewingkey')

  Result:
  amount              (numeric) the total balance of shield funds (in Sapling addresses)
  ```
* `importsaplingkey`
  ```
  Adds a key (as returned by exportsaplingkey) to your wallet.

  Arguments:
  1. "key"           (string, required) The zkey (see exportsaplingkey)
  2. rescan          (string, optional, default="whenkeyisnew") Rescan the wallet for transactions - can be "yes", "no" or "whenkeyisnew"
  3. startHeight     (numeric, optional, default=0) Block height to start rescan from

  Note: This call can take minutes to complete if rescan is true.

  Result:
  {
    "address" : "address|DefaultAddress",    (string) The address corresponding to the spending key (the default address).
  }
  ```
* `importsaplingviewingkey`
  ```
  Adds a viewing key (as returned by exportsaplingviewingkey) to your wallet.

  Arguments:
  1. "vkey"           (string, required) The viewing key (see exportsaplingviewingkey)
  2. rescan           (string, optional, default="whenkeyisnew") Rescan the wallet for transactions - can be "yes", "no" or "whenkeyisnew"
  3. startHeight      (numeric, optional, default=0) Block height to start rescan from

  Note: This call can take minutes to complete if rescan is true.

  Result:
  {
    "address" : "address|DefaultAddress",    (string) The address corresponding to the viewing key (for Sapling, this is the default address).
  }
  ```
* `listreceivedbyshieldaddress`
  ```
  Return a list of amounts received by a shield addr belonging to the node's wallet.

  Arguments:
  1. "address"      (string) The private address.
  2. minconf        (numeric, optional, default=1) Only include transactions confirmed at least this many times.

  Result:
  {
    "txid": "txid",            (string) the transaction id
    "amount": xxxxx,           (numeric) the amount of value in the note
    "memo": xxxxx,             (string) hexadecimal string representation of memo field
    "confirmations" : n,       (numeric) the number of confirmations
    "blockheight": n,          (numeric) The block height containing the transaction
    "blockindex": n,           (numeric) The block index containing the transaction.
    "blocktime": xxx,          (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).
    "outindex" (sapling) : n,  (numeric) the output index
    "change": true|false,      (boolean) true if the address that received the note is also one of the sending addresses
  }
  ```
* `listshieldaddresses`
  ```
  Returns the list of shield addresses belonging to the wallet.

  Arguments:
  1. includeWatchonly (bool, optional, default=false) Also include watchonly addresses (see 'importviewingkey')

  Result:
  [                 (json array of string)
    "addr",         (string) a shield address belonging to the wallet
    ...
  ]
  ```
* `listshieldunspent`
  ```
  Returns array of unspent shield notes with between minconf and maxconf (inclusive) confirmations.
  Optionally filter to only include notes sent to specified addresses.
  When minconf is 0, unspent notes with zero confirmations are returned, even though they are not immediately spendable.

  Arguments:
  1. minconf          (numeric, optional, default=1) The minimum confirmations to filter
  2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter
  3. includeWatchonly (bool, optional, default=false) Also include watchonly addresses (see 'importsaplingviewingkey')
  4. "addresses"      (string) A json array of shield addrs to filter on.  Duplicate addresses not allowed.
     [
       "address",     (string) shield addr
       ...
     ]

  Result:
  [                             (array of json object)
    {
      "txid" : "txid",          (string) the transaction id
      "outindex" (sapling) : n,       (numeric) the output index
      "confirmations" : n,       (numeric) the number of confirmations
      "spendable" : true|false,  (boolean) true if note can be spent by wallet, false if address is watchonly
      "address" : "address",    (string) the shield address
      "amount": xxxxx,          (numeric) the amount of value in the note
      "memo": xxxxx,            (string) hexademical string representation of memo field
      "change": true|false,     (boolean) true if the address that received the note is also one of the sending addresses
      "nullifier": xxxxx,       (string) the note's nullifier, hex encoded  }
    },
    ...
  ]
  ```
* `rawshieldsendmany`
  ```
  Creates a transaction sending to many recipients (without committing it), and returns the hex string.
  Amounts are decimal numbers with at most 8 digits of precision.
  Change generated from a transparent addr flows to a new  transparent addr address, while change generated from a shield addr returns to itself.
  When sending coinbase UTXOs to a shield addr, change is not allowed. The entire value of the UTXO(s) must be consumed.
  Requires wallet passphrase to be set with walletpassphrase call.

  Arguments:
  1. "fromaddress"        (string, required) The transparent addr or shield addr to send the funds from.
     It can also be the string "from_transparent"|"from_shield" to send the funds
     from any transparent|shield address available.
     Additionally, it can be the string "from_trans_cold" to select transparent funds,
     possibly including delegated coins, if needed.
  2. "amounts"            (array, required) An array of json objects representing the amounts to send.
     [{
       "address":address  (string, required) The address is a transparent addr or shield addr
       "amount":amount    (numeric, required) The numeric amount in HMS is the value
       "memo":memo        (string, optional) If the address is a shield addr, message string of max 512 bytes
     }, ... ]
  3. minconf              (numeric, optional, default=1) Only use funds confirmed at least this many times.
  4. fee                  (numeric, optional), The fee amount to attach to this transaction.
     If not specified, the wallet will try to compute the minimum possible fee for a shield TX,
     based on the expected transaction size and the current value of -minRelayTxFee.

  Result:
  transaction                (string) hex string of the transaction
  ```
* `shieldsendmany`
  ```
  Send to many recipients. Amounts are decimal numbers with at most 8 digits of precision.
  Change generated from a transparent addr flows to a new  transparent addr address, while change generated from a shield addr returns to itself.
  When sending coinbase UTXOs to a shield addr, change is not allowed. The entire value of the UTXO(s) must be consumed.
  Requires wallet passphrase to be set with walletpassphrase call.

  Arguments:
  1. "fromaddress"         (string, required) The transparent addr or shield addr to send the funds from.
     It can also be the string "from_transparent"|"from_shield" to send the funds
     from any transparent|shield address available.
     Additionally, it can be the string "from_trans_cold" to select transparent funds,
     possibly including delegated coins, if needed.
  2. "amounts"             (array, required) An array of json objects representing the amounts to send.
     [{
       "address":address  (string, required) The address is a transparent addr or shield addr
       "amount":amount    (numeric, required) The numeric amount in HMS is the value
       "memo":memo        (string, optional) If the address is a shield addr, message string of max 512 bytes
     }, ... ]
  3. minconf               (numeric, optional, default=1) Only use funds confirmed at least this many times.
  4. fee                   (numeric, optional), The fee amount to attach to this transaction.
     If not specified, the wallet will try to compute the minimum possible fee for a shield TX,
     based on the expected transaction size and the current value of -minRelayTxFee.

  Result:
  "id"          (string) transaction hash in the network
  ```
* `viewshieldtransaction`
  ```
  Get detailed shield information about in-wallet transaction "txid"

  Requires wallet passphrase to be set with walletpassphrase call.

  Arguments:
  1. "txid"    (string, required) The transaction id

  Result:
  {
    "txid" : "transactionid",   (string) The transaction id
    "fee"  : x.xxx,             (numeric) The transaction fee in HMS
    "spends" : [
      {
        "spend" : n,                    (numeric, sapling) the index of the spend within vShieldedSpend
        "txidPrev" : "transactionid",   (string) The id for the transaction this note was created in
        "outputPrev" : n,               (numeric, sapling) the index of the output within the vShieldedOutput
        "address" : "hemisaddress",      (string) The hemis address involved in the transaction
        "value" : x.xxx                 (numeric) The amount in HMS
        "valueSat" : xxxx               (numeric) The amount in satoshis
      }
      ,...
    ],
    "outputs" : [
      {
        "output" : n,                   (numeric, sapling) the index of the output within the vShieldedOutput
        "address" : "hemisaddress",      (string) The hemis address involved in the transaction
        "outgoing" : true|false         (boolean, sapling) True if the output is not for an address in the wallet
        "value" : x.xxx                 (numeric) The amount in HMS
        "valueSat" : xxxx               (numeric) The amount in satoshis
        "memo" : "hexmemo",             (string) Hexademical string representation of the memo field
        "memoStr" : "memo",             (string) Only returned if memo contains valid UTF-8 text.
      }
      ,...
    ],
  }
  ```
* `getsupplyinfo`
  ```
  If forceupdate=false (default if no argument is given): return the last cached money supply
  (sum of spendable transaction outputs) and the height of the chain when it was last updated
  (it is updated periodically, whenever the chainstate is flushed).

  If forceupdate=true: Flush the chainstate to disk and return the money supply updated to
  the current chain height.

  Arguments:
  1. forceupdate       (boolean, optional, default=false) flush chainstate to disk and update cache

  Result:
  {
    "updateheight" : n,       (numeric) The chain height when the transparent supply was updated
    "transparentsupply" : n   (numeric) The sum of all spendable transaction outputs at height updateheight
    "shieldsupply": n         (numeric) Chain tip shield pool value
    "totalsupply": n          (numeric) The sum of transparentsupply and shieldsupply
  }
  ```
* `initgamemaster`
  ```
  Initialize gamemaster on demand if it's not already initialized.

  Arguments:
  1. gamemasterPrivKey          (string, required) The gamemaster private key.
  2. gamemasterAddr             (string, required) The IP:Port of this gamemaster.

  Result:
  success                      (string) if the gamemaster initialization succeeded.
  ```
* `getcachedblockhashes`
  ```
  Return the block hashes cached in the gamemaster manager

  Result:
  [
    ...
    "xxxx",   (string) hash at Index d (height modulo max cache size)
    ...
  ]
  ```

#### Changed RPC Commands

Several RPC commands have had changes to their input arguments or output fields since v4.3.0 to support new functionality. Below is a detailed list of changes to existing RPC commands:

* `dumpwallet`
  A new entry (`warning`) is added to the return object, explaining the risks connected with sharing the output of the command. Also the filename can no longer contain the strings `"bug"` and `"log"`.

* `delegatestake`/`rawdelegatestake`
  A new input parameter (`fUseShielded`) has been added, in order to delegate funds from Shield addresses.

* `getblock`
  A new JSON entry (`finalsaplingroot`) has been added to the return object. (string) The root of the Sapling commitment tree after applying this block.

* `getblockheader`
  A new JSON object (`shield_pool_value`) has been added to the return object with the following information:
  ```
  "shield_pool_value":   (object) Block shield pool value
  {
     "chainValue":        (numeric) Total value held by the Sapling circuit up to and including this block
     "valueDelta":        (numeric) Change in value held by the Sapling circuit over this block
  }
  ```

* `getblockchaininfo`
  A new JSON entry (`initial_block_downloading`) has been added to the return object. (boolean) whether the node is in initial block downloading state or not.
  A new JSON object (`shield_pool_value`) has been added to the return object with the following information:
  ```
  "shield_pool_value":   (object) Chain tip shield pool value
  {
     "chainValue":        (numeric) Total value held by the Sapling circuit up to and including this block
     "valueDelta":        (numeric) Change in value held by the Sapling circuit over this block
  }
  ```

* `getblockindexstats`
  The `fFeeOnly` input argument has been removed.
  The `spendcount` and `pubspendcount` return objects have been removed due to zerocoin deprecation.

* `preparebudget` and `submitbudget`
  A maximum of `6` payment cycles per proposal (`20` on testnet) is now imposed for newly created proposals. Amounts higher than this limit will now return an error message.

* `getbudgetprojection`
  A typo in the JSON return fields has been corrected; `Alloted` and `TotalBudgetAlloted` have been renamed to `Allotted` and `TotalBudgetAllotted` respectively.

* `gmfinalbudget show`
  The `strName` key of the JSON object, representing a finalized budget in the output list, has now the format `"main" (Hash)`. The object no longer has the `"Hash"` entry.

* `sendtoaddress`/`sendmany`
  Shield addresses can be used as recipients. In this case the request is forwarded to `shieldsendmany` with first parameter set to `from_transparent`.

* `startgamemaster`
  A new optional input argument (`reload_conf`) has been added to reload the client's `gamemaster.conf` file in real-time. This optional argument is only available when running `startgamemaster` with "alias" being specified as the first argument.

* `decodegamemasterbroadcast`
  The `nlastdsq` JSON return entry has been removed. This was a remnant from pre-zerocoin "CoinJoin" mixing.

* `getinfo`
  The `zerocoinbalance` JSON return entry and the `zHMSsupply` return JSON object have been removed due to zerocoin deprecation.
  The `balance` JSON return entry is now all inclusive (the sum of all transparent and shield HMS)
  The `moneysupply` JSON return entry is the total HMS supply (the sum of all transparent and shield HMS)
  A new JSON return entry (`transparentsupply`) has been added to return the sum of the value of all unspent outputs when the chainstate was last flushed to disk.
  A new JSON return entry (`shieldsupply`) has been added to return the shield supply at the chain tip.
  Integrators should switch to the newly added `getsupplyinfo` RPC command for the purpose of obtaining supply information as it is more robust and does not rely on information caching.

* `validateaddress`
  The `account` JSON return entry has been removed due to the removal of the internal accounting system. The `label` entry was it's replacement.

* `getrawtransaction`
  A new JSON return entry (`type`) has been added which indicates via a numeric value the type of transaction (currently not fully implemented).
  A new JSON return array (`shield_addresses`) has been added which will contain a list of shield address strings involved in the transaction (if that information is available to the local client).
  Two new JSON return entries (`extraPayloadSize` and `extraPayload`) have been added pre-emptively to support future implementation of "special" transactions (currently NYI).

* `getrawtransaction` and `decoderawtransaction`
  New JSON output entries have been added to both these commands to detail sapling input/output information after the `vout` array in the following format:
  ```
  "valueBalance": n,          (numeric) The net value of spend transfers minus output transfers
  "valueBalanceSat": n,       (numeric) `valueBalance` in sats
  "vShieldSpend": [               (array of json objects)
     {
       "cv": "hex",         (string) A value commitment to the value of the input note
       "anchor": hex,         (string) A Merkle root of the Sapling note commitment tree at some block height in the past
       "nullifier": hex,       (string) The nullifier of the input note
       "rk": hex,              (string) The randomized public key for spendAuthSig
       "proof": hex,           (string) A zero-knowledge proof using the spend circuit
       "spendAuthSig": hex,    (string) A signature authorizing this spend
     }
     ,...
  ],
  "vShieldOutput": [             (array of json objects)
     {
       "cv": hex,                  (string) A value commitment to the value of the output note
       "cmu": hex,                 (string) The u-coordinate of the note commitment for the output note
       "ephemeralKey": hex,         (string) A Jubjub public key
       "encCiphertext": hex,       (string) A ciphertext component for the encrypted output note
       "outCiphertext": hex,       (string) A ciphertext component for the encrypted output note
       "proof": hex,               (string) A zero-knowledge proof using the output circuit
     }
     ,...
  ],
  "bindingSig": hex,       (string) Prove consistency of valueBalance with the value commitments in spend descriptions and output descriptions, and proves knowledge of the randomness used for the spend and output value commitments
  ```

* `getbalance`
  The `account` input argument has been removed due to the removal of the internal accounting system.
  A new optional input argument (`includeShield`) has been added to include shield balance. Default is `true`.

* `getcoldstakingbalance` and `getdelegatedbalance`
  The optional `account` input argument has been removed due to the removal of the internal accounting system.

* `listreceivedbyaddress`, `listreceivedbylabel`, `listsinceblock`, and `gettransaction`
  The JSON return entry `account` has been removed due to the removal of the internal accounting system.
  The JSON return entry `bcconfirmations` has been marked as deprecated and will be removed in the future.

* `listunspent`
  The JSON return entry `account` has been removed due to the removal of the internal accounting system.
  A new JSON entry (`generated`) has been added to indicate if the txout is a coinstake transaction.


#### Removed RPC Commands

The table below details RPC commands that have now been fully removed. These commands had previously been deprecated or otherwise marked for removal with the v5.0 release:

| Command Name | Previous Purpose | Reason for removal |
| ------------ | ---------------- | ------------------ |
| `findserial` | Searches the zerocoin database for a zerocoin spend transaction that contains the specified serial | Zerocoin deprecation |
| `getserials` | Look at the inputs of any tx in a range of blocks and returns the serial numbers for any coinspend | Zerocoin deprecation |
| `getaccountaddress` | Returned the current hemis address associated with an internal account | Account system removal |
| `setaccount` | Associate a hemis address with an internal account name | Account system removal |
| `getaccount` | Returned the internal account associated with a given hemis address | Account system removal |
| `getaddressesbyaccount` | Returned a list of hemis addresses associated with an internal account | Account system removal |
| `sendtoaddressix` | Send an amount to a given address using SwiftX | SwiftX disabled |
| `getreceivedbyaccount` | Returned the total amount received by addresses associated with an internal account | Account system removal |
| `move` | Move funds from one internal account to another internal account | Account system removal |
| `sendfrom` | Send an amount from an internal account to a hemis address | Account system removal |
| `listreceivedbyaccount` | List received transactions by account | Account system removal |
| `listaccounts` | List internal account names and balances | Account system removal |
| `multisend` | Configure sending a portion of stake rewards to a hemis address | Pending a full rewrite |
| `getzerocoinbalance` | Returned the wallet's total zHMS balance | Zerocoin deprecation |
| `listmintedzerocoins` | List all zHMS mints in the wallet | Zerocoin deprecation |
| `listzerocoinamounts` | List information about your zerocoin amounts | Zerocoin deprecation |
| `listspentzerocoins` | List all the spent zHMS mints in the wallet | Zerocoin deprecation |
| `mintzerocoin` | Mint the specified zHMS amount | Zerocoin deprecation |
| `spendzerocoin` | Spend zHMS to a HMS address | Zerocoin deprecation |
| `spendzerocoinmints` | Spend specific zHMS mints to a HMS address | Zerocoin deprecation |
| `resetmintzerocoin` | Scan the chain for zerocoin mints held in the wallet DB and refresh their status | Zerocoin deprecation |
| `resetspentzerocoin` | Scan the chain for all zerocoin spends held in the wallet DB and refresh their status | Zerocoin deprecation |
| `getarchivedzerocoin` | Display zerocoins that were archived because they were believed to be orphans | Zerocoin deprecation |
| `exportzerocoins` | Export zerocoin mints that are held in the wallet DB | Zerocoin deprecation |
| `importzerocoins` | Import zerocoin mints | Zerocoin deprecation |
| `reconsiderzerocoins` | Check archived zHMSs to see if any mints were added to the chain | Zerocoin deprecation |
| `setzpivseed` | Set the wallet's deterministic zHMS seed to a specific value | Zerocoin deprecation |
| `getzpivseed` | Return the current deterministic zHMS seed | Zerocoin deprecation |
| `generatemintlist` | Show mints that are derived from the deterministic zHMS seed | Zerocoin deprecation |
| `dzpivstate` | Show the current state of the mintpool of the wallet | Zerocoin deprecation |
| `searchdzpiv` | Do a search for deterministically generated zHMS that have not yet added to the wallet | Zerocoin deprecation |
| `spendrawzerocoin` | Create and broadcast a TX spending the provided zericoin | Zerocoin deprecation |


*v5.0.0* Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### GUI
- #1857 `981a5f86f4` [GUI] do not decode the address twice in save contact. (furszy)
- #1863 `ad15bce2f5` [Trivial][GUI] Fix init messages (random-zebra)
- #1881 `5d66d5c296` [GUI] Autocomplete 'help' PRC commands in UI Consoles (Fuzzbawls)
- #1895 `a418e9f1a3` [Bug][GUI] Fix wallet crashing on faq-buttons press (random-zebra)
- #1900 `aafd0541c2` [QT] fixing qt flag deprecation (furszy)
- #1919 `df2a6dbf60` [GUI] isStakingStatusActive pointers guarded. (furszy)
- #1928 `15632533c3` [GUI] Double confirmation dialog for dumpwallet command. (furszy)
- #1931 `65ca5e0255` [Bug][GUI] guarding wallet ptr access in WalletModel:getEncryptionStatus. (furszy)
- #1963 `94c182e320` [Core] Sapling milestone 5: Graphical User Interface (furszy)
- #1977 `bc2ec8590f` [GUI] Coin control dialog utxo processing/update flow large cleanup and improvements (furszy)
- #1978 `f6a1a6e0e3` [GUI] FAQ "Exit" moved to "Close FAQ" (furszy)
- #1997 `58f7562529` [Bug][GUI] CControl: cache utxo amount values properly (random-zebra)
- #1999 `0805834485` [GUI] Encrypted memo feature implemented (furszy)
- #2003 `8a07799388` [GUI] Tx detail, don't show inputs if there is no information for them. (furszy)
- #2004 `fe7c942272` [BugFix] [GUI] Shield all transparent balance fee calculation. (furszy)
- #2005 `6fb1dd7084` [GUI] Fixing export addressbook (furszy)
- #2008 `7fe89170d5` [Bug] label of coin type in send widget after shielded coin control (random-zebra)
- #2010 `0371b10bbb` [Bug][GUI] Encrypted memo with transparent recipient address (random-zebra)
- #2011 `e79fbccc97` [Bug][GUI] memo not reset after hitting reset button (random-zebra)
- #2017 `0fe1242ee6` [GUI] Guard shielded flows pre-v5 enforcement (furszy)
- #2025 `2768e98dc6` [GUI] Cold staking screen, hide memo entry button. (furszy)
- #2030 `f3a753d4c8` [GUI] Workaround to the GM MISSING/REMOVE labeling (furszy)
- #2033 `5b252c059f` [GUI][Trivial] Adjust CoinControl column widths for readability (Ambassador)
- #2035 `f06f2cbf24` [Bug][GUI] TxDetail dialog memo: plain text edit + copy button (furszy)
- #2037 `17a39c0c1e` [GUI] Address generation error on locked wallets fix plus extra corrections. (furszy)
- #2039 `d6b7d1837f` [GUI] Periodic make translate (Fuzzbawls)
- #2055 `1ff296441b` [GUI] Transaction detail, scroll memo fix, starting from the top. (furszy)
- #2066 `fe6cc349d0` [Bug][GUI] Receive widget: check typeRole before refreshing view (random-zebra)
- #2070 `6557613c09` [GUI] coin-control: remove column checkbox extra white space in tree-mode (random-zebra)
- #2073 `8d4f64906f` [GUI] Dashboard, fix type filtering disappearance. (furszy)
- #2075 `1d2d335f20` [GUI] Settings information (furszy)
- #2094 `c0863655d3` [GUI] Hide shield all coins button. (furszy)
- #2096 `98a487eed4` [GUI][BugFix] Duplicate transaction records. (furszy)
- #2098 `6d8ee7eb29` [Trivial][GUI] coin control: Fixed column size in list-mode (random-zebra)
- #2100 `b9eb39b2f2` [GUI] Support for op_return proposal output records. (furszy)
- #2102 `fb71ddffa2` [BUG][GUI] Cache unconfirmed_balance only for transparent outs (random-zebra)
- #2104 `b9665fb9c0` [BUG][GUI] Don't return StakingOnlyUnlocked from WalletModel::sendCoins (random-zebra)
- #2111 `e217d1e306` [BUG][GUI] coin-control dialog: more accurate "after fee" + fDust for shield recipient (random-zebra)
- #2115 `4634bb6193` [GUI] Fixing tooltip balance amount cropped (furszy)
- #2121 `7aa80011b9` [GUI][Trivial] Save cold-staking address in contacts (random-zebra)
- #2122 `670b94a6b8` [BUG][GUI] Fix double conf dialog for RPC commands that expose privkeys (random-zebra)

### Wallet
- #1814 `2018fca21d` [Core][Wallet] Sapling transaction primitive data. (furszy)
- #1819 `b6562473aa` [Wallet] Address book encapsulation. (furszy)
- #1850 `9520ee370b` [wallet] Extra GetTransaction call in stakeable coins selection flow removed. (furszy)
- #1868 `fb94ca77d4` [wallet] GetGamemasterVinAndKeys wasn't locking cs_wallet to obtain the collateral. (furszy)
- #1884 `ee1be563a2` [Wallet] Sapling keys management final round. (furszy)
- #1911 `c9f16d09ad` [Wallet] Kill Accounts (Fuzzbawls)
- #1918 `638c146a8f` [Wallet] Sapling notes, nullifiers and witnesses management. (furszy)
- #1943 `46fad30bbb` [Wallet][DB] Back ports up to the CWallet/CWalletDB/CDB refactor. (furszy)
- #1948 `2037003b68` [Bug][Wallet] Lock cs_main before accessing CheckFinalTx (random-zebra)
- #1951 `cec4b92afe` [Wallet] CWalletTx: missing fStakeDelegationVoided initialization (furszy)
- #1952 `c7f7b009b7` [Wallet] Shielded transactions balance cache system. (furszy)
- #1959 `eaac8fe2e7` [Wallet] [Wallet] fix GetShieldedChange, tx isSapling renamed to isSaplingVersion (furszy)
- #1961 `740f958bea` [Wallet] Sapling operation new spend from several different sources feature + send/build decoupling. (furszy)
- #1985 `7c7f1bf916` [Cleanup] Remove zerocoin wallet (random-zebra)
- #1989 `da927fc31b` [Sapling][Bug] Fixing multi-source notes spending (furszy)
- #2007 `a0d5b62e5b` [Wallet] Sapling: don't update witnesses for external notes (random-zebra)
- #2013 `7a4153f7ce` [Wallet] Initial cleanup to the v1 transparent CreateTransaction flow. (furszy)
- #2020 `8d2ab3ba9d` [Wallet] HD wallets automatic upgrade to sapling features. (furszy)
- #2027 `1ee90a49fe` [Sapling] Try to recover corruption of notes cache during send operation (random-zebra)
- #2046 `7bec5ce8f8` [Sapling] Transparent inputs selection (furszy)
- #2048 `d6e5f281ea` [Wallet] BugFix: automatic upgrade for unlocked wallet (furszy)
- #2065 `75d52340ee` [Wallet][Sapling] Dummy ECDSA sigs and zk-proofs during fee calculation loop (random-zebra)
- #2080 `9f3ec5ba89` [BUG][Wallet] return unconfirmed balance for all spendable coins by default (random-zebra)
- #2109 `57c807e342` [Wallet] clear StakeableCoins before initializing (PeterL73)

### RPC
- #1869 `525a45a3a9` [RPC] Update univalue and unify JSON obj pushes (Fuzzbawls)
- #1878 `7bba3947af` [Core][RPC][Bug] Use sum of utxo values as money supply (random-zebra)
- #1894 `f02fcc1b9a` [RPC][Bug] Fix 'gmfinalbudget show' returning a single budget (random-zebra)
- #1906 `051719e2af` [RPC] Cache money supply on memory. Introduce getsupplyinfo (random-zebra)
- #1909 `48e3c91e8c` [Bug][RPC] Guard RPC server during warm-up (random-zebra)
- #1926 `ab460a523f` [RPC][Refactor] Register calls where they are defined (Fuzzbawls)
- #1927 `bf04df311e` [RPC] Add additional warning to dumpwallet result object (Fuzzbawls)
- #1930 `2dc9704ba2` [RPC] Speeding up listgamemasters command (furszy)
- #1949 `d36963415d` [Bug][RPC] Add missing lock in sendrawtransaction (random-zebra)
- #1964 `a0afd9f625` [RPC] shielded_sendmany from multiple sources + raw_shielded_sendmany (random-zebra)
- #1965 `56b133f283` [RPC][Tests] Add sapling mempool test + getbestsaplinganchor (random-zebra)
- #1970 `692d053d91` [Validation][RPC] Delegate from shielded addresses (random-zebra)
- #1971 `98d2443c2e` [RPC] [scripted-diff] Don't use snake case in RPC command names (Fuzzbawls)
- #1972 `880f1afcb0` [RPC][Tests] Memo field for sapling transactions (random-zebra)
- #1982 `3156f92be2` [RPC] shielded supply in getsupplyinfo and getinfo (random-zebra)
- #2014 `b01a1bb2a3` [RPC] redirect sendmany to shieldedsendmany when recipient is shielded (random-zebra)
- #2018 `51fe307f13` [Bug][RPC] Save collateral errors into proposals/budgets strInvalid (random-zebra)
- #2047 `ea872d3763` [Bug][RPC] Fix check in wallet upgrade RPC (random-zebra)
- #2049 `6a2fb3e417` [RPC] Add nullifiers to listshieldedunspent output (random-zebra)
- #2050 `bc01a697eb` [RPC] redirect sendtoaddress to shieldedsendmany for shielded recipient (random-zebra)
- #2086 `1a410bc848` [RPC] Use "shield" nomenclature instead of "shielded" (Fuzzbawls)
- #2088 `23f6fab207` [RPC] Cleanup RPC help output (Fuzzbawls)
- #2093 `9d0923f699` [Trivial][RPC] Drop 'ed' suffix from vShielded* in TxSaplingToJSON (random-zebra)
- #2097 `5867b88a4d` [RPC] Fix more RPC help output (Fuzzbawls)
- #2113 `00fa7da1fa` [Doc] RPC getrawtransaction and decoderawtransaction (furszy)
- #2119 `cf043ad47d` [RPC] Fix inconsistencies in outputs (Fuzzbawls)
- #2120 `2a736a89a4` [RPC] Update Sapling HD chain seed with sethdseed (random-zebra)

### Gamemasters/Budget
- #1829 `0adce3cbb7` [Gamemasters] Tier two network sync new architecture, regtest support + GM activation functional test. (furszy)
- #1834 `9d5f5952e0` [Gamemasters] CActiveGamemaster::ManageStatus remove unneded GetAvailableBalance check (furszy)
- #1842 `41927dfe6c` [Refactoring] Faster gm Check() - remove AcceptableInputs (random-zebra)
- #1844 `7196a35651` [Refactoring] Budget, round 2: review locks (random-zebra)
- #1845 `f152deed36` [Refactoring] Budget, round 3: split UpdateValid checks for proposals / finalized budgets (random-zebra)
- #1851 `24c9dbf38e` [Refactoring] Budget, round 4: remove Broadcast classes (random-zebra)
- #1855 `3cd52771f2` [Bug] wrong reserveKey when committing budget/proposal collaterals (random-zebra)
- #1858 `951470bd43` [Refactoring] Budget, round 5: proposals/budgets FeeTX indexes (random-zebra)
- #1861 `8f37bcccc3` [Refactoring] Budget, round 6: gamemaster vote for finalized budgets (random-zebra)
- #1862 `70c1fd1981` [Refactoring] Budget, round 7: split gamemaster-budget files (random-zebra)
- #1886 `db9b0933d5` [Gamemasters] dead end over the activation process. (furszy)
- #1891 `5b9f25e5f9` [Bug] Set budget best height before reading budget DB during init (random-zebra)
- #1968 `47916129bc` [TierTwo] Missing misbehaving + ban score not introduced on the new msg dispatcher flow (furszy)
- #2006 `9ce3fec43c` [Refactor] Gamemaster activeState on-demand (random-zebra)
- #2023 `f9a29a9205` [Bug] Fix lock order issue with CMMan::CheckAndRemove and CMPayments::CleanPaymentList (random-zebra)
- #2024 `a307a26eb6` [Bug] GetLastPaid/SecondsSincePayment locking cs_main (random-zebra)
- #2031 `e7fcab0019` [Gamemasters] remove recursive lock in CGamemaster::Status() (random-zebra)
- #2045 `1617f9d347` [TierTwo] Remove unneeded extra logging. (furszy)
- #2056 `945044c740` [Gamemaster] Encapsulate global and do not parse privkey redundantly everywhere. (furszy)
- #2060 `c246834912` [Bug][GM] Immediately detect forked gamemasters (random-zebra)
- #2068 `6db9e9c189` [Consensus] Budget: Fix instant killing of proposals (random-zebra)
- #2084 `11606ee0d4` [BUG][Gamemaster] Miner not paying valid finalized budget if <20 active gamemasters (random-zebra)
- #2103 `7a1837eb58` [Startup] Do not block local peer port selection for Gamemasters running over tor. (furszy)

### Core
- #1798 `21fdc49327` [Core][Wallet] Sapling Integration: Chain + Wallet transaction manager. (furszy)
- #1848 `98ab596778` [Bug] Fix misaligned height in calls to GetBlockValue (random-zebra)
- #1867 `c65d0368a0` [Bug] g_IsSaplingActive flag is not initialized at startup. (furszy)
- #1870 `696e328d8c` [Sapling] Incremental merkle tree backport. (furszy)
- #1872 `2f4e1caac1` [Core] Mrs Peacock in The Library with The Candlestick (killed main.{h,cpp}) (random-zebra)
- #1875 `8e7fa721af` [Bug] tx sync parsing problem fixed + extra refactoring. (furszy)
- #1879 `717c53c23c` [BugFix] Migrate list serialization to the current flow. (furszy)
- #1888 `d7bb310f0c` [Bug] Fix regression with emplace_back on cold-staking scripts (random-zebra)
- #1890 `bec361e86f` [Bug][Script] Add fColdStaking boolean argument to IsSolvable (random-zebra)
- #1903 `f1b9b42046` [Sapling] Anchors and nullifiers integrated into the chain state view layered cache. (furszy)
- #1904 `92035f80fe` [Validation] Introduce rolling cache for block hashes in gamemaster manager (random-zebra)
- #1910 `d28fef17c7` [Validation] v8 blocks, Sapling merkle tree inclusion. (furszy)
- #1912 `ba9c3eec06` [Sapling] Sapling transaction: builder, sapData net validation and builder unit tests coverage (furszy)
- #1913 `0a820797fb` [Mempool] Add tags to mempool's mapTx indices (Suhas Daftuar)
- #1914 `017396d2aa` [Bug] Swiftx DoConsensusVote missing brackets always returning without doing anything. (furszy)
- #1915 `8a9b92c0ed` [Validation] Fix block-connection performance regression and mempool not copying tx performance improvement (Matt Corallo)
- #1921 `14f0e974f8` [Core] Lock cs_vSend and cs_inventory in a consistent order even in TRY (random-zebra)
- #1937 `3bf6248633` [Refactor] Removing prev index pointer from FillBlockPayee flow. (furszy)
- #1938 `de5b240881` [Core] Arg parsing overhaul (Fuzzbawls)
- #1940 `2a192ea87e` [Validation] Introducing Sapling transaction network connection and validations. (furszy)
- #1945 `d98378e56b` [Core] Say "Goodbye" to SwiftTX (random-zebra)
- #1953 `1b7d182bb5` [Cleanup] Stop using Boost where possible (Fuzzbawls)
- #1955 `072081538c` [Core] Sapling transaction version (random-zebra)
- #1956 `c76192ff1c` [Bug] Fix transaction size limits for sapling version (random-zebra)
- #1958 `efa4a94d80` [Validation] Sapling nullifiers mempool connection (random-zebra)
- #1962 `911a046859` [Bug] Updated stakeable coins in staker thread (random-zebra)
- #1966 `0ad68a3dbe` [Core] Special Transactions base layer (random-zebra)
- #1967 `226aaaa9d2` [Refactor] SaplingOperation: support for multisig, cold-staking, and OP_RETURN outputs (random-zebra)
- #1975 `7837dd4d62` [Trivial] Log changed count for sapling nullifiers and anchors (random-zebra)
- #1976 `9e3c70d34a` [Consensus] Introduce SPORK_19 and SPORK_20 (random-zebra)
- #1979 `738e5023fe` [Consensus][Validation] Dynamic Fee for Shielded Transactions + Max Size. (random-zebra)
- #1980 `6f2f2e876b` [Sapling] Remove Sprout parameters (random-zebra)
- #1981 `3f6f501c80` [Cleanup][Sapling] Final Sprout cleanup (random-zebra)
- #1988 `2f1dcfce44` [Trivial] Rename placeholder for v5 network upgrade info (random-zebra)
- #1993 `e576a1fc99` [Validation] v5 zerocoin final deprecation. (furszy)
- #1994 `78726e46f3` [Consensus] Bump active protocol version to 70920 (furszy)
- #1998 `547d60690e` [Bug] g_IsSapling active flag during reindex (random-zebra)
- #2000 `0756462149` [Core] Gamemaster collateral index (furszy)
- #2009 `3ec4f66a8b` [Trivial] constant functions in SaplingScriptPubKeyMan (random-zebra)
- #2034 `5b02228cf4` [SSPKM] Crash fix, external notes witness decrement (furszy)
- #2038 `27bd2dcd33` [Consensus] Set final enforcement height for V5 on Testnet (random-zebra)
- #2051 `2746bad417` [Consensus] Bump TxVersion::SAPLING to 3 (random-zebra)
- #2052 `e04debc359` [Miner] Update block header's current version to v8 (Fuzzbawls)
- #2058 `9e2d82504c` [Cleanup] Removing unused DisconnectBlocks and ReprocessBlocks (furszy)
- #2062 `30cc7f66c5` [Consensus] Max block space available for shielded txes (random-zebra)
- #2063 `84ca60a42f` [Backport] Fix races for strMiscWarning and fLargeWork*Found (furszy)
- #2064 `d3866a08a0` [Script][Bug] Fix signature malleability for t inputs in Sapling txes (random-zebra)
- #2067 `05a7668b5c` [Consensus][Validation] Reject outdated block version after v5 enforcement (random-zebra)
- #2069 `1f98c2b28d` [Consensus] Start new Testnet (v5) (random-zebra)
- #2071 `dc7eb08ff4` [Consensus] Update Spork keys for v5 (random-zebra)
- #2083 `95ed10a37e` [Core] Prevector Optimizations (Lenny Maiorani)
- #2091 `7a256942e3` [Trivial][Refactor] Pass caught logic_error by reference in CreateSig (random-zebra)
- #2101 `9d95c16e5b` [Consensus] Max payment count for budget proposals (random-zebra)
- #2108 `0477b7c3ca` [Policy] Set DEFAULT_SHIELDEDTXFEE_K to 100 (from 1000) (random-zebra)
- #2112 `e7283dec9e` [Consensus] Set v5 enforcement height for mainnet (furszy)

### Build Systems
- #1856 `1bbb906059` [CMake] Add test_hemis unit test target (Fuzzbawls)
- #1877 `a59e8273fb` [Misc] Fixing recurrent compiler warnings (furszy)
- #1882 `5bd7d52c2c` [Build][Doc] Post branch-off/release maintenance (Fuzzbawls)
- #1883 `e7d78bd2c2` [Build] Clean lcov artifacts from all paths (Fuzzbawls)
- #1942 `e63d8290f5` [Travis] Increase Travis ccache size and reduce spam (Fuzzbawls)
- #1947 `1fd7a25fc3` [Travis] Set stricter timeout for Dash 32bit job (Fuzzbawls)
- #1986 `cf4c971151` [Build] Replicate TravisCI tests in Github Actions (Fuzzbawls)
- #1990 `027c036f0a` [Build] Pass proper commit range to linter (Fuzzbawls)
- #1991 `06b28f1854` [Build] Fix a few GA issues (Fuzzbawls)
- #2001 `ab081d0060` [Build] Address more issues with GA (Fuzzbawls)
- #2021 `3845c18723` [GA] Reduce number of concurrent jobs (Fuzzbawls)
- #2022 `8385a96f94` [Build] Package sapling params in repo/release (Fuzzbawls)
- #2036 `a3e75c2afa` [Build] Proper coverage for ppa (Fuzzbawls)
- #2041 `a15dc3417d` [Build] CMake hemisd and unit tests: fixing missing MacOS framework. (furszy)
- #2087 `869b621c2c` [Build] Unify header file extensions (Fuzzbawls)

### P2P/Network
- #2012 `a45e827c60` [Backport] P2P open network connection (Matt Corallo)
- #2015 `860064b890` [Net] Prevent un-necessary disconnections in version handshake (Fuzzbawls)
- #2028 `007359a632` [P2P] Add missing lock in CAddrMan::Clear() (practicalswift)
- #2029 `57fa1341f6` [P2P] When clearing addrman clear mapInfo and mapAddr (Gregory Sanders)
- #2054 `d9cbbad1dc` [Net] Socks5 netbase back ports (furszy)

### Testing
- #1871 `3c05a7bd2d` [Sapling] Note Encryption unit tests back ported. (furszy)
- #1885 `40278cb688` [Test] Update sighash tests for shielded transactions. (furszy)
- #1887 `901480fc01` [Test] Updating and enabling bip32 unit test (furszy)
- #1889 `4136d8f5a6` [Test] unit test checking cold staking script key extraction. (furszy)
- #1916 `6d8525169f` [Test] Introducing tier two functional test suite + several fixes (furszy)
- #1944 `2dc773b1e1` [Tests] Split out zerocoin and sapling unit tests to their own targets (Fuzzbawls)
- #1946 `98c6183d72` [Tests] Testing framework, adding mocktime startup arg (furszy)
- #1969 `437ea2d691` [Tests] Add support for sapling txes and v8 blocks in the framework (random-zebra)
- #1983 `18ee5cee1a` [Tests][Cleanup] Nuke zerocoin from the functional test framework (random-zebra)
- #1987 `4ee2ad2e09` [Tests] Add sapling_supply functional test (random-zebra)
- #1995 `b09752dc69` [Tests] Fix double Bug in sapling_mempool functional test (random-zebra)
- #2016 `564dce8f82` [Tests] Add --all option to test_runner for complete functional tests coverage (random-zebra)
- #2042 `8b04db16ff` [Test] Adding sapling transaction validation unit test coverage. (furszy)
- #2059 `a4fd2e1311` [Test] Make rpc_ban unit test pass again (Bernhard M. Wiedemann)
- #2081 `2e6aeed60a` [Tests] minor fixes for mining_pos_reorg.py (random-zebra)
- #2116 `4e438d8d26` [Test] Print error if shield diversified address is equal to the default address (furszy)
- #2123 `9d935a9646` [Test] Attempt to fix the diversified address equal to default address test scenario (furszy)

### Cleanup/Refactoring
- #1815 `7b2b9d048e` [Refactor] Make CBlock a vector of shared_ptr of CTransactions (furszy)
- #1830 `a9946da47d` [Refactor] Emplace elements in maps and vectors (random-zebra)
- #1837 `d1682ef4f6` [Refactor] Use Optional template in guiinterface.h (Kolby Moroz Liebl)
- #1853 `e8d13ef4b0` [Cleanup] Removing unused and unneeded functions and members (furszy)
- #1866 `585c9fed6d` [Refactor] Decouple peer-processing from block-connection logic (Matt Corallo)
- #1898 `4edb8c5d8e` [Refactor] Pass previous index from AcceptBlock to AcceptBlockHeader (random-zebra)
- #1901 `443318460f` [Backport] test: remove glibc fdelt sanity check (furszy)
- #1905 `ea5f85e071` [Cleanup] removing unused mapRelay and cs_mapRelay. (furszy)
- #1922 `2d86a41951` [Refactor] Strip out cs_main lock for misbehaving from Gamemasterman:ProcessMessage (furszy)
- #1924 `65c493313e` [Cleanup] Remove unconfirmed/immature zerocoin balance calculations (random-zebra)
- #1932 `f092c28c76` [Trivial] Remove unused var in SaplingSPKM::AddSaplingSpendingKey (random-zebra)
- #1933 `79f15c6141` [Cleanup] Remove duplicate header includes (Fuzzbawls)
- #1934 `c576916cf2` [Cleanup] Remove ZLNP remnants in bloom.h/bloom.cpp (random-zebra)
- #1936 `ec54459192` [Cleanup] Remove Q_FOREACH (Fuzzbawls)
- #1950 `ce702dabb8` [Backport] Moving GetDustThreshold out of transaction primitive (furszy)
- #1954 `676f6a79bf` [Refactor] Move CFeeRate out of the consensus module (random-zebra)
- #2026 `ee792245ae` [Cleanup] [Trivial] Remove stale siphash.py file (Fuzzbawls)
- #2061 `7082d8899c` [Trivial] Add Zcash copyright statement (random-zebra)

### Docs/Output
- #1860 `5aed03f6fe` [Logs] Missing gmping category added to logcategories (furszy)
- #2032 `18a91240f6` [Doc] Windows WSL build recommendation to temporarily disable Win32 PE support (Aaron Clauson)
- #2095 `b9ed70ea5b` [Doc] Main readme re-written (furszy)

## Credits

Thanks to everyone who directly contributed to this release:
- Aaron Clauson
- Ambassador
- Ben Woosley
- Bernhard M. Wiedemann
- Fuzzbawls
- Gregory Sanders
- Jonas Schnelli
- Kolby Moroz Liebl
- Lenny Maiorani
- MarcoFalke
- Matt Corallo
- Pavel Janík
- PeterL73
- Philip Kaufmann
- Pieter Wuille
- Suhas Daftuar
- Wladimir J. van der Laan
- furszy
- gnuser
- practicalswift
- random-zebra

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.