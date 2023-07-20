hemis Core version 2.3.1 is now available from:

  <https://github.com/hemis-project/hemis/releases>

This is a new minor version release, including various bug fixes and
performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/hemis-project/hemis/issues>

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

Notable Changes
===============

RPC changes
--------------

#### Update of RPC commands to comply with the forthcoming RPC Standards PIP ####

| Old Command | New Command | Notes |
| --- | --- | --- |
| `masternode count` | `getmasternodecount` | |
| `masternode list` | `listmasternodes` | |
| `masternodelist` | `listmasternodes` | renamed |
| `masternode connect` | `masternodeconnect` | |
| `masternode current` | `getcurrentmasternode` | |
| `masternode debug` | `masternodedebug` | |
| `masternode enforce` |  | removed |
| `masternode outputs` | `getmasternodeoutputs` | |
| `masternode status` | `getmasternodestatus` | |
| `masternode list-conf` | `listmasternodeconf` | added optional filter |
| `masternode genkey` | `createmasternodekey` | |
| `masternode winners` | `listmasternodewinners` | |
| `masternode start` | `startmasternode` | see notes below |
| `masternode start-alias` | `startmasternode` | see notes below |
| `masternode start-<mode>` | `startmasternode` | see notes below |
| `masternode create` | | removed - not implemented |
| `masternode calcscore` | `listmasternodescores` | |
| --- | --- | --- |
| `mnbudget prepare` | `preparebudget` | see notes below |
| `mnbudget submit` | `submitbudget` | see notes below |
| `mnbudget vote-many` | `mnbudgetvote` | see notes below |
| `mnbudget vote-alias` | `mnbudgetvote` | see notes below |
| `mnbudget vote` | `mnbudgetvote` | see notes below |
| `mnbudget getvotes` | `getbudgetvotes` | |
| `mnbudget getinfo` | `getbudgetinfo` | see notes below |
| `mnbudget show` | `getbudgetinfo` | see notes below |
| `mnbudget projection` | `getbudgetprojection` | |
| `mnbudget check` | `checkbudgets` | |
| `mnbudget nextblock` | `getnextsuperblock` | |

##### `startmasternode` Command #####
This command now handles all cases for starting a masternode instead of having multiple commands based on the context. Command arguments have changed slightly to allow the user to decide wither or not to re-lock the wallet after the command is run. Below is the help documentation:

```
startmasternode "local|all|many|missing|disabled|alias" lockwallet ( "alias" )

 Attempts to start one or more masternode(s)

Arguments:
1. set         (string, required) Specify which set of masternode(s) to start.
2. lockWallet  (boolean, required) Lock wallet after completion.
3. alias       (string) Masternode alias. Required if using 'alias' as the set.

Result: (for 'local' set):
"status"     (string) Masternode status message

Result: (for other sets):
{
  "overall": "xxxx",     (string) Overall status message
  "detail": [
    {
      "node": "xxxx",    (string) Node name or alias
      "result": "xxxx",  (string) 'success' or 'failed'
      "error": "xxxx"    (string) Error message, if failed
    }
    ,...
  ]
}

Examples:
> hemis-cli startmasternode "alias" true "my_mn"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "startmasternode", "params": ["alias" true "my_mn"] }' -H 'content-type: text/plain;' http://127.0.0.1:51473/
```

##### `preparebudget` & `submitbudget` Commands #####
Due to the requirement of maintaining backwards compatibility with the legacy command, these two new commands are created to handle the preparation/submission of budget proposals. Future intention is to roll these two commands back into a single command to reduce code-duplication. Paramater arguments currently remain unchanged from the legacy command equivilent.

##### `mnbudgetvote` Command #####
This command now handles all cases for submitting MN votes on a budget proposal. Backwards compatibility with the legacy command(s) has been retained, with the exception of the `vote-alias` case due to a conflict in paramater type casting. A user running `mnbudget vote-alias` will be instructed to instead use the new `mnvote` command. Below is the full help documentation for this new command:

```
mnbudgetvote "local|many|alias" "votehash" "yes|no" ( "alias" )

Vote on a budget proposal

Arguments:
1. "mode"      (string, required) The voting mode. 'local' for voting directly from a masternode, 'many' for voting with a MN controller and casting the same vote for each MN, 'alias' for voting with a MN controller and casting a vote for a single MN
2. "votehash"  (string, required) The vote hash for the proposal
3. "votecast"  (string, required) Your vote. 'yes' to vote for the proposal, 'no' to vote against
4. "alias"     (string, required for 'alias' mode) The MN alias to cast a vote for.

Result:
{
  "overall": "xxxx",      (string) The overall status message for the vote cast
  "detail": [
    {
      "node": "xxxx",      (string) 'local' or the MN alias
      "result": "xxxx",    (string) Either 'Success' or 'Failed'
      "error": "xxxx",     (string) Error message, if vote failed
    }
    ,...
  ]
}

Examples:
> hemis-cli mnbudgetvote "local" "ed2f83cedee59a91406f5f47ec4d60bf5a7f9ee6293913c82976bd2d3a658041" "yes"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "mnbudgetvote", "params": ["local" "ed2f83cedee59a91406f5f47ec4d60bf5a7f9ee6293913c82976bd2d3a658041" "yes"] }' -H 'content-type: text/plain;' http://127.0.0.1:51473/
```

##### `getbudgetinfo` Command #####
This command now combines the old `mnbudget show` and `mnbudget getinfo` commands to reduce code duplication while still maintaining backwards compatibility with the legacy commands. Given no parameters, it returns the full list of budget proposals (`mnbudget show`). A single optional parameter allows to return information on just that proposal (`mnbudget getinfo`). Below is the full help documentation:

```
getbudgetinfo ( "proposal" )

Show current masternode budgets

Arguments:
1. "proposal"    (string, optional) Proposal name

Result:
[
  {
    "Name": "xxxx",               (string) Proposal Name
    "URL": "xxxx",                (string) Proposal URL
    "Hash": "xxxx",               (string) Proposal vote hash
    "FeeHash": "xxxx",            (string) Proposal fee hash
    "BlockStart": n,              (numeric) Proposal starting block
    "BlockEnd": n,                (numeric) Proposal ending block
    "TotalPaymentCount": n,       (numeric) Number of payments
    "RemainingPaymentCount": n,   (numeric) Number of remaining payments
    "PaymentAddress": "xxxx",     (string) hemis address of payment
    "Ratio": x.xxx,               (numeric) Ratio of yeas vs nays
    "Yeas": n,                    (numeric) Number of yea votes
    "Nays": n,                    (numeric) Number of nay votes
    "Abstains": n,                (numeric) Number of abstains
    "TotalPayment": xxx.xxx,      (numeric) Total payment amount
    "MonthlyPayment": xxx.xxx,    (numeric) Monthly payment amount
    "IsEstablished": true|false,  (boolean) Established (true) or (false)
    "IsValid": true|false,        (boolean) Valid (true) or Invalid (false)
    "IsValidReason": "xxxx",      (string) Error message, if any
    "fValid": true|false,         (boolean) Valid (true) or Invalid (false)
  }
  ,...
]

Examples:
> hemis-cli getbudgetinfo
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getbudgetinfo", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:51473/
```

#### Masternode network protocol layer reporting ####
The results from the `listmasternodes` and `getmasternodecount` commands now includes details about which network protocol layer is being used (IPv4, IPV6, or Tor).


2.3.1 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, refactors and string updates. For convenience in locating
the code changes and accompanying discussion, both the pull request and
git merge commit are mentioned.

### RPC and other APIs
- #239 `e8b92f4` [RPC] Make 'masternode status' more verbose (Mrs-X)
- #244 `eac60dd` [RPC] Standardize RPC Commands (Fuzzbawls)

### P2P Protocol and Network Code
- #248 `0d44ca2` [core] fix payment disagreements, reduce log-verbosity (Mrs-X)

### Miscellaneous
- #240 `1957445` [Debug Log] Increase verbosity of error-message (Mrs-X)
- #241 #249 `b60118b` `7405e31` Nullpointer reference fixed (Mrs-X)

Credits
=======

Thanks to everyone who directly contributed to this release:
- Fuzzbawls
- Mrs-X
- amirabrams

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/).
