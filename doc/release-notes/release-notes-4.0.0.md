hemis Core version *v4.0.0* is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>


Mandatory Update
==============

hemis Core v4.0.0 is a mandatory update for all users. This release contains new consensus rules and improvements that are not backwards compatible with older versions. Users will need to update their clients before enforcement of this update goes into effect.

Update enforcement is currently scheduled to go into effect at the following time:

```
Mainnet: Sunday, January 5, 2020 12:00:00 AM GMT
```

Gamemasters will need to be restarted once both the gamemaster daemon and the controller wallet have been upgraded.

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

New Wallet UI
-------------------

v4.0.0 introduces a completely new GUI for the wallet, designed and coded from the ground up by the [Krubit](https://krubit.com/) team.

This new UI, aside from the overall design large implementation, includes user-focused improvements and features such as a brief introduction on first load, a FAQ section, one-click QRCode compatible receiving addresses, gamemaster creation wizard, dark and light themes, filterable staking charts, and much more.

You can read more details about this extensive work in ([PR #954](https://github.com/hemis-Project/hemis/pull/954))

There are some legacy features that have not been included, however, notably the in-wallet block explorer and the governance page. The in-wallet block explorer was sorely outdated, and the governance page was a newer addition that will be seeing a return in a future version.

Cold Staking
-------------------

A brand new feature is being introduced with the release of v4.0.0: Cold Staking ([PR #955](https://github.com/hemis-Project/hemis/pull/955))! This feature allows a coin owner to keep coins in a "cold" (or locked) wallet whilst a "hot" wallet carries out the burden of staking those coins.

This brings added security to coin owners as they are no longer required to use an unlocked or partially unlocked wallet (with the ability to spend coins anywhere) in order to gain staking rewards. Users who have chosen to store their coins on hardware devices such as a Ledger or Trezor<sup>1</sup> can also gain staking rewards with those coins.

A full technical writup is available on the [hemis Wiki](https://github.com/hemis-Project/hemis/wiki/ColdStaking), and an initial video showcase is available on [YouTube](https://www.youtube.com/watch?v=utxB5TzAeXc).
A brief guide to setup cold staking with GUI and RPC is available [here](https://github.com/random-zebra/hemis-Wiki/blob/master/User-Documentation/Cold-Staking-HowTo.md).

<sup>1</sup> Spending cold stakes from HW wallets currently available only for Ledger devices via [PET4L](https://github.com/hemis-Project/PET4L) tool.

Multi-Split Stake Splitting
-------------------

Stake splitting has received a makeover and now supports splitting to more than two (2) outputs. [PR #968](https://github.com/hemis-Project/hemis/pull/968) introduced the change, which is controlled by the wallet's `stakesplitthreshold` setting.

The default split threshold remains at 2000 HMS, and can be adjusted in the GUI's Settings page, or via the RPC `setstakesplitthreshold` command.

For a real example, with a stake split threshold of 1500, and a UTXO of 4708.1557; the current stake split algorithm would break that into two outputs of approximately 2355.07785. With this new logic; it will be broken into 3 outputs instead of two; each sized 1570.0519 (4708.1557 input + 2 stake = 4710.1557 / 3 outputs = 1570.0519.

The maximum number of outputs is currently capped at 48. Also, in relation to the new Cold Staking feature described above; the stake split threshold is set by the staker wallet and **NOT** the owner wallet.

New Consensus Rules
-------------------

The following consensus rule changes will be enforced on or shortly after block `2153200`. Note that **Upgrade Enforcement** (mentioned above) will occur prior to this block height.

### V1 zHMS Spending (Public Spends Version 4)

Since the discovery of a critical exploit within the libzerocoin library in early 2019, remaining legacy v1 zHMS have been un-spendable. We're happy to say that, once the new consensus rules are in effect, users will once again be able to spend their v1 zHMS with public spends version 4 ([PR #936](https://github.com/hemis-Project/hemis/pull/936)).

As with the previous version 3 public spends introduced in core wallet version 3.3.0 (enabling the spending of v2 zHMS), version 4 spends will also be public. A full technical writeup is available on the [hemis Wiki](https://github.com/hemis-Project/hemis/wiki/CoinRandomnessSchnorrSignature).

### OP_CHECKCOLDSTAKEVERIFY and P2CS

Cold staking introduces a new opcode, `OP_CHECKCOLDSTAKEVERIFY`, in the scripting language, and a new standard transaction type using it, named `P2CS` (Pay-To-Cold-Staking). A P2CS script is defined as follows:
```
OP_DUP OP_HASH160 OP_ROT OP_IF OP_CHECKCOLDSTAKEVERIFY [HASH160(stakerPubKey]
OP_ELSE [HASH160(ownerPubKey)] OP_ENDIF OP_EQUALVERIFY OP_CHECKSIG
```
`OP_CHECKCOLDSTAKEVERIFY` is used to ensure that the staker can only spend the output in a valid coinstake transaction (using the same P2CS script in the new output).

### Time Protocol v2

[#PR1002](https://github.com/hemis-Project/hemis/pull/1002) introduces a new time protocol for the Proof-Of-Stake consensus mechanism, to ensure better efficiency, fairness and security. The time is now divided in 15-seconds slots and valid blocktimes are at the beginning of each slot (i.e. the block timestamp's seconds can only be `00`, or `15`, or `30` or `45`).<br>
The maximum future time limit is lowered from 3 minutes to 14 seconds and the past limit is set to the previous blocktime (i.e. a block can no longer have a timestamp earlier than its previous block).<br>
This means that, when looking for a valid kernel, each stakeable input can be hashed only once every 15 seconds (once per timeslot), and it is not possible to submit blocks with timestamp higher than the current time slot. This ultimately enables the removal of the "hashdrift" concept.<br>

**NOTE:** Given the much stricter time constraints, a node's clock synchronization is required for P2P connections: the maximum time offset is 15 seconds and peers with a time drift higher than 30 seconds (in absolute value) will be outright disconnected.

 For advanced users, we recommend the setup of NTP clients and own servers. This will provide to your node a higher level of time accuracy and the best, time wise, synchronization with the peers in the network.

### Block Version 7

[#PR1022](https://github.com/hemis-Project/hemis/pull/1022) defines Version 7 blocks, which remove the (now-unused) accumulator checkpoint from the block header. This results in an overall data reduction of ~256 bits from each block as well as the in-memory indexes.

### New Network Message Signatures

Layer 2 network messages (GM, Budget, Spork, etc) are now signed based on the hash of their **binary** content instead of their **string** representation ([#PR1024](https://github.com/hemis-Project/hemis/pull/1024)).

### New SPORKS

Two new SPORKS are introduced, `SPORK_17` ([#PR975](https://github.com/hemis-Project/hemis/pull/975)) and `SPORK_18` ([#PR995](https://github.com/hemis-Project/hemis/pull/995)).<br>
`SPORK_17` (off by default) is used to activate the [Cold Staking](#cold-staking) protocol. When this spork is off, no cold-staked block is accepted by the network and new delegations are rejected, but coin-owners are still able to spend previously created pay-to-cold-stake delegations.

`SPORK_18` (off by default) is used to switch between Version 3 and [Version 4 Public Spends](#v1-zpiv-spending-public-spends-version-4). When this spork is active, only version 4 spends are accepted by the network. When it's not, only version 3 spends are accepted.

RPC Changes
--------------

### New options for existing wallet commands

A new (optional) argument, `includeDelegated`, has been added to the following commands that allows these commands to include delegated coins/information in their operation:
- `getbalance` - Boolean (Default: True)
- `sendfrom` - Boolean (Default: False)
- `sendmany` - Boolean (Default: False)
- `listtransactions` - Boolean (Default: True)

Additionally, a new (optional) argument, `includeCold`, has been added to the `listtransactions` command (Boolean - Default: True), which allows for filtering of cold-staker delegated transactions.

### New return fields for existing commands

The `validateaddress` command now includes an additional response field, `isstaking`, to indicate wither or not the specified address is a cold staking address.

The `getwalletinfo` command now includes two additional response fields:
- `delegated_balance` - HMS balance held in P2CS contracts (delegated amount total).
- `cold_staking_balance` - HMS balance held in cold staking addresses.

### Newly introduced commands

The following new commands have been added to the RPC interface:
- `getnewstakingaddress`
- `delegatestake`
- `rawdelegatestake`
- `getcoldstakingbalance`
- `delegatoradd`
- `delegatorremove`
- `listcoldutxos`
- `liststakingaddresses`
- `listdelegators`

Details about each new command can be found below.

`getnewstakingaddress` generates a new cold staking address:
```
getnewstakingaddress ( "account" )

Returns a new hemis cold staking address for receiving delegated cold stakes.

Arguments:
1. "account"        (string, optional) The account name for the address to be linked to. if not provided, the default account "" is used. It can also be set to the empty string "" to represent the default account. The account does not need to exist, it will be created if there is no account by the given name.

Result:
"hemisaddress"    (string) The new hemis address
```

`delegatestake` sends a cold staking delegation transaction:
```
delegatestake "stakingaddress" amount ( "owneraddress" fExternalOwner fUseDelegated fForceNotEnabled )

Delegate an amount to a given address for cold staking. The amount is a real and is rounded to the nearest 0.00000001

Requires wallet passphrase to be set with walletpassphrase call.

Arguments:
1. "stakingaddress"      (string, required) The hemis staking address to delegate.
2. "amount"              (numeric, required) The amount in HMS to delegate for staking. eg 100
3. "owneraddress"        (string, optional) The hemis address corresponding to the key that will be able to spend the stake.
                               If not provided, or empty string, a new wallet address is generated.
4. "fExternalOwner"      (boolean, optional, default = false) use the provided 'owneraddress' anyway, even if not present in this wallet.
                               WARNING: The owner of the keys to 'owneraddress' will be the only one allowed to spend these coins.
5. "fUseDelegated"       (boolean, optional, default = false) include already delegated inputs if needed.6. "fForceNotEnabled"    (boolean, optional, default = false) force the creation even if SPORK 17 is disabled (for tests).

Result:
{
   "owner_address": "xxx"   (string) The owner (delegator) owneraddress.
   "staker_address": "xxx"  (string) The cold staker (delegate) stakingaddress.
   "txid": "xxx"            (string) The stake delegation transaction id.
}
```

`rawdelegatestake` creates a raw cold staking delegation transaction without broadcasting it to the network:
```
rawdelegatestake "stakingaddress" amount ( "owneraddress" fExternalOwner fUseDelegated )

Delegate an amount to a given address for cold staking. The amount is a real and is rounded to the nearest 0.00000001

Delegate transaction is returned as json object.
Requires wallet passphrase to be set with walletpassphrase call.

Arguments:
1. "stakingaddress"      (string, required) The hemis staking address to delegate.
2. "amount"              (numeric, required) The amount in HMS to delegate for staking. eg 100
3. "owneraddress"        (string, optional) The hemis address corresponding to the key that will be able to spend the stake.
                               If not provided, or empty string, a new wallet address is generated.
4. "fExternalOwner"      (boolean, optional, default = false) use the provided 'owneraddress' anyway, even if not present in this wallet.
                               WARNING: The owner of the keys to 'owneraddress' will be the only one allowed to spend these coins.
5. "fUseDelegated         (boolean, optional, default = false) include already delegated inputs if needed.

Result:
{
  "txid" : "id",        (string) The transaction id (same as provided)
  "version" : n,          (numeric) The version
  "size" : n,             (numeric) The serialized transaction size
  "locktime" : ttt,       (numeric) The lock time
  "vin" : [               (array of json objects)
     {
       "txid": "id",    (string) The transaction id
       "vout": n,         (numeric)
       "scriptSig": {     (json object) The script
         "asm": "asm",  (string) asm
         "hex": "hex"   (string) hex
       },
       "sequence": n      (numeric) The script sequence number
     }
     ,...
  ],
  "vout" : [              (array of json objects)
     {
       "value" : x.xxx,            (numeric) The value in btc
       "n" : n,                    (numeric) index
       "scriptPubKey" : {          (json object)
         "asm" : "asm",          (string) the asm
         "hex" : "hex",          (string) the hex
         "reqSigs" : n,            (numeric) The required sigs
         "type" : "pubkeyhash",  (string) The type, eg 'pubkeyhash'
         "addresses" : [           (json array of string)
           "hemisaddress"        (string) hemis address
           ,...
         ]
       }
     }
     ,...
  ],
  "hex" : "data",       (string) The serialized, hex-encoded data for 'txid'
}
```

`getcoldstakingbalance` returns the cold balance of the wallet:
```
getcoldstakingbalance ( "account" )

If account is not specified, returns the server's total available cold balance.
If account is specified, returns the cold balance in the account.
Note that the account "" is not the same as leaving the parameter out.
The server total may be different to the balance in the default "" account.

Arguments:
1. "account"      (string, optional) The selected account, or "*" for entire wallet. It may be the default account using "".

Result:
amount              (numeric) The total amount in HMS received for this account in P2CS contracts.
```

`delegatoradd` whitelists a delegated owner address for cold staking:
```
delegatoradd "addr"

Add the provided address <addr> into the allowed delegators AddressBook.
This enables the staking of coins delegated to this wallet, owned by <addr>

Arguments:
1. "addr"        (string, required) The address to whitelist

Result:
true|false           (boolean) true if successful.
```

`delegatorremove` to remove previously whitelisted owner address:
```
delegatoradd "addr"

Add the provided address <addr> into the allowed delegators AddressBook.
This enables the staking of coins delegated to this wallet, owned by <addr>

Arguments:
1. "addr"        (string, required) The address to whitelist

Result:
true|false           (boolean) true if successful.
```

`listcoldutxos` lists all P2CS UTXOs belonging to the wallet (both delegator and cold staker):
```
listcoldutxos ( nonWhitelistedOnly )

List P2CS unspent outputs received by this wallet as cold-staker-

Arguments:
1. nonWhitelistedOnly   (boolean, optional, default=false) Whether to exclude P2CS from whitelisted delegators.

Result:
[
  {
    "txid" : "true",            (string) The transaction id of the P2CS utxo
    "txidn" : "accountname",    (string) The output number of the P2CS utxo
    "amount" : x.xxx,             (numeric) The amount of the P2CS utxo
    "confirmations" : n           (numeric) The number of confirmations of the P2CS utxo
    "cold-staker" : n             (string) The cold-staker address of the P2CS utxo
    "coin-owner" : n              (string) The coin-owner address of the P2CS utxo
    "whitelisted" : n             (string) "true"/"false" coin-owner in delegator whitelist
  }
  ,...
]
```

`liststakingaddresses` lists all cold staking addresses generated by the wallet:
```
liststakingaddresses "addr"

Shows the list of staking addresses for this wallet.

Result:
[
   {
   "label": "yyy",  (string) account label
   "address": "xxx",  (string) hemis address string
   }
  ...
]
```

`listdelegators` lists the whitelisted owner addresses
```
listdelegators "addr"

Shows the list of allowed delegator addresses for cold staking.

Result:
[
   {
   "label": "yyy",  (string) account label
   "address": "xxx",  (string) hemis address string
   }
  ...
]
```

Snapcraft Packages
------------------

For our linux users, in addition to the [Ubuntu PPA](https://launchpad.net/~hemis) repository, we are now offering a [Snap package](https://snapcraft.io/hemis-core) as quick way to install and update a hemis wallet.

Release versions are available via the `Stable` branch, and (for testing-only purposes) nightly builds are available in the `Beta` branch.

Internal Miner/Staker Change
--------------

The wallet's internal miner/staker is no longer prevented from running prior to having synced all the additional layer 2 (GM/Budget) data. Instead, mining/staking uses better logic to allow block creation without fully synced layer 2 data when the full data set wouldn't be required.

In other words, try to stake a new block only if:

- full layer 2 sync is complete
OR
- The spork list is synced and all three sporks (8,9 and 13) are **not** active.

Faster Shutdown During Initial Loading
--------------

Previously, if a user wanted to close/quit the wallet before it had finished its initial loading process, they would need to wait until that loading process actually completed before the wallet would fully close.

Now, the new behavior is to gracefully close the wallet once the current step is complete.

*v4.0.0* Change log
==============

Detailed release notes follow. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Core
- #643 `469d974519` [Crypto] Use stronger rand for key generation (warrows)
- #936 `12a6b704b6` [zHMS] PublicCoinSpend v4 - Coin Randomness Schnorr Signature (random-zebra)
- #955 `008b7938db` [Core][Script][Wallet][RPC][Tests] Cold Staking (random-zebra)
- #989 `6f645ce457` [DB] Db runtime error cleaning the variable that needs to be logged (furszy)
- #1000 `34e11dd5fa` [Core] Spork code overhaul (random-zebra)
- #1002 `5666184cc5` [PoS] Time Protocol v2 (random-zebra)
- #1022 `b5bede0661` remove accumulators checkpoint (random-zebra)
- #1025 `988ee3fe37` [Startup] Stop loading the wallet if shutdown was requested. (furszy)
- #1029 `0df20ddbab` [Startup][Refactor][Backport] Memory allocation fail handler + init step 1 refactored. (furszy)
- #1040 `01fe200d4a` [Bug] Fix GetDepthInMainChain returning 0 when tx is not in mempool (random-zebra)
- #1050 `adc74f737c` [Core] Prevent coinstakes from overpaying gamemasters (random-zebra)
- #1063 `04834fff67` [Node] Remove a call to IsSuperMajority (warrows)
- #1066 `6a4bf7c42c` [Cleanup][Refactor]Main.cpp code cleanup. (furszy)
- #1067 `c947e534ee` [Node] Replace IsSuperMajority with height checks (warrows)
- #1070 `fbffae1b38` [Bug] Fix contextCheckBlock for the first  block that it's a v1 block. (furszy)
- #1129 `a87bfc32a0` [Consensus] Define TestNet changeover block for hemis v4.0 RC (random-zebra)
- #1134 `a4ded20de4` [Trivial] Remove a duplicate variable definition (warrows)
- #1191 `9a054eeba6` [Consensus] Define MainNet changeover block for hemis v4.0 (random-zebra)
- #1197 `ad241150e9` [Trivial] Update copyright headers (Fuzzbawls)

### GUI
- #954 `e815815fdc` [GUI] [Model] New Wallet UI (furszy)
- #997 `4d50ff33da` [Trivial][UI] GM screen (furszy)
- #998 `6380da7eb4` [Qt] Guard GM tab from possible missing TXs in gamemaster.conf (Fuzzbawls)
- #999 `c6567aec89` [UI] Send screen total amounts refresh after custom options cleaned (furszy)
- #1003 `a89f4e6603` [QT][Performance] Memory leak re creating the row object instead of re initialize it. (furszy)
- #1006 `ac523b24c2` [Trivial] Fix gmrow ifdef typo (Fuzzbawls)
- #1007 `39e8a03505` [Qt] Reintroduce networkstyle to title texts (Fuzzbawls)
- #1008 `91514a0326` [Qt] Don't set placeholder on QPlainTextEdit (Fuzzbawls)
- #1009 `e0178087c8` [QT] Dashboard chart left and right day range movement control buttons (furszy)
- #1012 `0d05b5381d` [Performance][Wallet][QT][Model] TransactionTableModel multi-thread initialization + tx filter sort speed improved. (furszy)
- #1014 `49f42e9a59` [GUI] Refresh coin control upon reopening (CryptoDev-Project)
- #1015 `17ef4163dc` [Qt] Update source strings (Fuzzbawls)
- #1016 `33182066ba` [GUI] Tx detail dialog (furszy)
- #1017 `8b6819a9d8` [GUI][Trivial] Remove hardcoded address. (Matias Furszyfer)
- #1018 `e264c77e63` [GUI] Update amounts when entry is removed from SendMultiRow (random-zebra)
- #1026 `968000bb33` [Cleanup] Prev 4.0 wallet UI files cleanup. (furszy)
- #1032 `d3b2969fff` [GUI] Dashboard chart map first segfault (furszy)
- #1033 `c04f442e4c` [GUI][Model][Wallet] Cold staking UI. (furszy)
- #1048 `1cb55b4822` [Qt] Make CoinControlTreeWidget focusable (Fuzzbawls)
- #1053 `6aaa531ec1` [GUI] Segfault for a bad cast of the parent in the escape key press event (furszy)
- #1054 `7c2cd32d6e` [UI] Guard a call to GetDepthInMainChain (warrows)
- #1055 `232cea5584` [Wallet] Create label for addresses generated via gamemaster wizard (CryptoDev-Project)
- #1057 `d4e6525410` [UI] Fix AA_EnableHighDpiScaling warning (Akshay)
- #1071 `297acac8d6` [UI] Settings options buttons hover css (furszy)
- #1072 `1e0cea53e8` [Qt] Update welcomecontentwidget.ui (Jeffrey)
- #1073 `64a90d717f` [Model][Backport] Remove mapWallet not needed call + stop treating coinbase differently (furszy)
- #1074 `54ff8bd3da` [UI] TransactionFilter do not invalidate filter if range is already set (furszy)
- #1075 `31e9f0fbde` [Model][UI] Receive dialog (furszy)
- #1076 `decac23124` [GUI] Decreasing the tooltip padding for #1076 windows issue. (furszy)
- #1083 `fba6d3cdac` [Qt] Hide option for 3rd party transaction URLs (Fuzzbawls)
- #1084 `7b3455acaa` [Qt] Show display unit option as text (Fuzzbawls)
- #1085 `a4d141fd1c` [Qt] Fixup topbar balance calculation (Fuzzbawls)
- #1099 `5d03160837` [GUI][Model][Performance] Dashboard (furszy)
- #1104 `97bea9cfc0` [Trivial][GUI] Fix typos in welcome widget (random-zebra)
- #1105 `e4e5df1018` [GUI] Fix bug in change wallet passphrase (random-zebra)
- #1109 `4771f6a7d7` [Trivial] [GUI] Customize Fee Dialog text change (NoobieDev12)
- #1112 `645854ad57` [Qt] Refresh coin control when re-opening from CS widget (Fuzzbawls)
- #1113 `b546151bc0` [Qt] Re-work settings restart and saving flow (Fuzzbawls)
- #1115 `12be3e2a8c` [Trivial] [GUI] Request Dialog typo fix (JSKitty)
- #1119 `6ceff8f97f` [GUI] Cold staking Warning for unconfirmed balance + stop multiple model updates (furszy)
- #1120 `0d04267c89` [GUI][BUG] Bad top padding on the dashboard nav icon in dark theme fix. (furszy)
- #1121 `0216fd3c8a` [GUI] Cold staking alert user if the owner address is not from the wallet (furszy)
- #1122 `641b1d6bbe` [GUI][Backport] Explicitly disable "Dark Mode" appearance on macOS (fanquake)
- #1123 `a4fb368d39` [GUI] Prevent worker constant creation and invalid removal (furszy)
- #1124 `bbb0125077` [GUI] Use QRegexValidator instead of the QDoubleValidator. (furszy)
- #1125 `c6e238ca4d` [GUI] Inform if open hemis.conf and/or backups folder fail. (furszy)
- #1126 `c910490c53` [BUG] Fix send transaction detail destinations (furszy)
- #1130 `c582cabbd3` [UI] Copy correct data from gminfo dialog (Akshay)
- #1131 `8ca5db691b` [Bug] URI read from file coded properly.. (furszy)
- #1132 `a34a7dfe1f` [Bug][GUI] Tx list row amount color changing invalidly. (furszy)
- #1133 `d159cb1878` [Bug][GUI] Topbar sync progressbar not expanding fixed. (furszy)
- #1139 `03b51c411d` [Qt] Fix segfault when running GUI client with --help or -? (Fuzzbawls)
- #1141 `01c517085b` [GUI][Model] isTestNetwork regtest correction. (furszy)
- #1142 `17ad5a2a61` [Bug] Fix segfault on GUI initialization for cold staker wallet (random-zebra)
- #1151 `da3bea5971` Rewording text under Change Wallet Passphrase (NoobieDev12)
- #1158 `7deae859f1` [BUG] Gamemasters wizard (furszy)
- #1159 `1ae53d0a2a` [GUI][Trivial] Rewording of Error message when wallet is unlocked for staking only (NoobieDev12)
- #1160 `88ddd1a947` [GUI][Model] Do not re request passphrase when the wallet is unlocked. (furszy)
- #1161 `e4bfe3e9af` [GUI][Trivial] Custom change address (furszy)
- #1162 `1c23ea6a59` [Qt] Periodic make translate (Fuzzbawls)
- #1163 `474e2bc6b2` [GUI] Validate wallet password on enter key press (warrows)
- #1165 `c54dad3b9f` [GUI][Bug] Cold staking screen (furszy)
- #1166 `4cc04b0ba6` [Qt] properly copy IPv6 externalip GM info (Fuzzbawls)
- #1173 `8e42e03192` [GUI][Trivial] Remove every pushButton focus decoration property. (furszy)
- #1174 `b352e2d096` [GUI] Min cold staking amount in ColdStaking widget (random-zebra)
- #1178 `d7a929c6a8` [GUI][Bug] Cold staking screen (furszy)
- #1179 `7a33fe10d2` [Qt] Fix for dead link to wrong hemis website (NoobieDev12)
- #1183 `60053d6786` [GUI][Trivial] Allow immediate typing in dialogs / tools widget (random-zebra)
- #1185 `682d54cd12` [GUI][Trivial] Make amount optional in staking address gen dialog (random-zebra)
- #1186 `0cc2976bbc` [GUI][Trivial] move caps lock warning in askpassphrase dialog (random-zebra)
- #1187 `0a2baa686d` [GUI][Trivial] Fix tx detail dialog expanding policy (random-zebra)
- #1188 `d5ed83896c` [GUI] Workaround to the OSX border-image pink stripes. (furszy)
- #1190 `129c4744ea` [GUI] Feature/Bug CoinControl Update on Open (Liquid369)
- #1192 `e52043b260` [GUI][Bug] Accept P2CS locked coins in coincontrol (random-zebra)
- #1193 `f49b0309a0` [GUI][Wallet] Allow spending of P2CS without coincontrol selection (random-zebra)
- #1194 `8854eace66` fix "total staking" amount (random-zebra)
- #1195 `ce5bc08988` [Qt] Update translations from transifex (Fuzzbawls)

### P2P and Network Code
- #975 `9765a772b8` [Consensus] Define SPORK_17 (random-zebra)
- #995 `8f2217d2bd` [Consensus] Define SPORK_18 (random-zebra)
- #1001 `59f55d6a54` [Consensus] Remove Old message format in CGamemasterBroadcast (random-zebra)
- #1024 `98aa3fa438` [Consensus] New signatures for network messages (random-zebra)
- #1110 `f89f672847` [Masterndoes] Gamemasters sync try locking cs_main when it looks for the tip (furszy)
- #1118 `3219d9b48c` [Sporks] Guard chainActive.Tip() and chainActive.Height() methods call. (furszy)
- #1128 `d6573e70c7` [Consensus] nTimeOffset warning for time protocol V2 (random-zebra)
- #1137 `4241574857` [Net] Protocol update enforcement for 70918 (random-zebra)
- #1138 `e0c49356ae` [Consensus] nTimeOffset warning addition (random-zebra)
- #1167 `fc4ffcf4af` [Trivial] Remove time offset warning when it gets back within range (random-zebra)
- #1177 `421cc1017b` [Network] Add SPORK 17 & 18 to the fMissingSporks flag + code reorg. (furszy)

### Wallet
- #968 `47ccf45adb` [Staking][Wallet] Add Multi-Split functionality to stake output splitting (Cave Spectre)
- #970 `0f1764a3db` [Wallet] Various transaction handling improvements (warrows)
- #1030 `ed83481494` [Wallet][Startup][DB][Backport] Remove vchDefaultKey from wallet.h (furszy)
- #1031 `52aed66930` [Wallet] fix CreateZerocoinSpendTransaction with empty addressesTo (random-zebra)
- #1039 `41d19c106f` Fix OOM when deserializing UTXO entries with invalid length (random-zebra)
- #1043 `d1d0cb691a` [Wallet][Tests] Fix bug re-adding orphan coinstake's inputs to the wallet (random-zebra)
- #1056 `3ba3b0e244` [Wallet][Refactor] Updating ancient getbalanceXXX methods to a lambda call. (furszy)
- #1058 `18e23a49e7` [Wallet] Transaction IsEquivalentTo method backported + code cleanup. (furszy)
- #1064 `bfbbb6b30c` [Wallet][RPC] Diagnose unsuitable outputs in lockunspent() (random-zebra)
- #1065 `d605e528db` [Wallet] Unlock spent outputs (random-zebra)
- #1068 `59b45e6d33` [Wallet] Do not cache the credit amount if fUnspent is enabled. (random-zebra)
- #1069 `d6f9eff763` [Wallet] Enable miner with gmsync incomplete (random-zebra)
- #1116 `e4d7addbb7` [Wallet] Do not use p2cs outputs in the autocombine flow. (furszy)
- #1117 `8d425bdf0d` [Wallet][GUI][Model] Cold staking addresses contacts flow. (furszy)

### Build System(s)
- #977 `ee9f6ca9da` [Build] CMake Improvements (Fuzzbawls)
- #979 `8ffc045f7d` [Compilation] Pass caught exceptions by reference (warrows)
- #990 `e7e31442e8` [Build] Fix wrong argument when verifying MacOS signatures (Mrs-X)
- #991 `4f3dd5ef66` [Build] Remove OpenSSL 1.0 check (warrows)
- #1010 `6ff12f70de` [Build] Fixup moc includes and generation (Fuzzbawls)
- #1013 `75b8ad2ae5` [Build] Clean up GUI dependency checking (Fuzzbawls)
- #1021 `899b4bd6a9` [Build] Use in-tree intermediary endian header (Fuzzbawls)
- #1023 `509d63526b` [Travis] Lower timeout for the full test suite (warrows)
- #1027 `569ac5e5e0` [Cleanup] Get rid of compiler warnings (random-zebra)
- #1036 `adfcb46149` [Build] Add SnapCraft Builds (Fuzzbawls)
- #1042 `8efd696b3f` [TravisCI] Run CMake tests earlier (Fuzzbawls)
- #1045 `a761323178` [travis] Update .travis.yml (Warrows)
- #1052 `dd62f3f66a` [CMake] Fix macOS Boost detection (Fuzzbawls)
- #1127 `c7bc2e1288` [Deployment] Windows taskbar icon pixelated fix. (furszy)
- #1136 `0f92bf8e17` [Build] Include full version in release file names (fanquake)
- #1157 `04480012aa` [Travis] Increase functional tests reserved time (Warrows)
- #1180 `4f4da456de` [Build] Add random-zebra gitian GPG public key fingerprint (random-zebra)

### RPC Interface
- #1111 `fcb0db8321` [Trivial] Fix help text for delegatorremove (Akshay)

### Testing System(s)
- #981 `92de4b81b3` [Tests] Add RPC budget regression tests (Fuzzbawls)

### Documentation
- #1051 `9ea7519026` [Doxygen] Generate Todo list (Fuzzbawls)
- #1176 `eb1eb4d2c6` [Docs][Utils] Overhaul gitian build docs/script (Fuzzbawls)
- #1181 `78034c937b` [Docs] 4.0.0 release notes (Fuzzbawls)

## Credits

Thanks to everyone who directly contributed to this release:
- Akshay
- Alex Morcos
- Andrew Chow
- Ben Woosley
- Cave Spectre
- Cory Fields
- CryptoDev-Project
- Dag Robole
- Fuzzbawls
- Gregory Solarte
- JSKitty
- James Hilliard
- Jeffrey
- Jonas Schnelli
- Liquid369
- MarcoFalke
- Matias Furszyfer
- Matt Corallo
- Mrs-X
- NoobieDev12
- Pieter Wuille
- Warrows
- Wladimir J. van der Laan
- cevap
- dexX7
- fanquake
- furszy
- practicalswift
- presstab
- random-zebra

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.