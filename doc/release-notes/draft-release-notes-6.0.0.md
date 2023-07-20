(note: this is a temporary file, to be added-to by anybody, and moved to release-notes at release time)

hemis Core version *version* is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>


How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).

Notable Changes
==============

(Developers: add your notes here as part of your pull requests whenever possible)


Deterministic Masternode Lists
------------------------------

hemis v6.0.0 introduces on-chain consensus for masternode lists, which allow for deterministic quorum derivation, implementing Dash's [DIP-0003](https://github.com/dashpay/dips/blob/master/dip-0003.md).

In the previous masternode system, each node needed to maintain their own individual masternode list with P2P messages, thus discrepancies might occur, for example, due to a different order of message reception.
Deterministic Masternode lists are lists of masternodes, built at every block, relying only on on-chain data (previous list, and transactions included in the current block).
All nodes derive (and verify) their masternode lists independently, from the same on-chain transactions, thus they immediately reach consensus on the tier-two state (number of masternodes, properties and status of each one).

Masternodes are "registered" by special transactions called ProTx, and removed only by spending the collateral.
A ProTx either creates a 10000-PIV collateral as tx output, or includes a reference to an unspent 10000-PIV utxo on chain (and a proof of ownership).
See PR [#2267](https://github.com/hemis-Project/hemis/pull/2267) for more information.

Upgrade instructions: !TODO

#### New RPC commands

* `protx_list`
    ```  
    protx_list (detailed wallet_only valid_only height)

    Lists all ProTxs.

    Arguments:
    1. detailed       (bool, optional, default=true) Return detailed information about each protx.
                          If set to false, return only the list of txids.
    2. wallet_only    (bool, optional, default=false) If set to true, return only protx which involves
                          keys from this wallet (collateral, owner, operator, voting, or payout addresses).
    3. valid_only     (bool, optional, default=false) If set to true, return only ProTx which are active/valid
                          at the height specified.
    4. height         (numeric, optional) If height is not specified, it defaults to the current chain-tip.

    Result:
    [...]                         (list) List of protx txids or, if detailed=true, list of json objects.
    ```

* `protx_register`
    ```
    protx_register "collateralHash" collateralIndex "ipAndPort" "ownerAddress" "operatorPubKey" "votingAddress" "payoutAddress" (operatorReward "operatorPayoutAddress")

    Creates and sends a ProTx to the network. The collateral is specified through "collateralHash" and collateralIndex, and must be an unspent
    transaction output spendable by this wallet. It must also not be used by any other masternode.

    Arguments:
    1. "collateralHash"        (string, required) The collateral transaction hash.
    2. collateralIndex         (numeric, required) The collateral transaction output index.
    3. "ipAndPort"             (string, required) IP and port in the form "IP:PORT".
                                  Must be unique on the network. Can be set to 0, which will require a ProUpServTx afterwards.
    4. "ownerAddress"          (string, required) The hemis address to use for payee updates and proposal voting.
                                  The private key belonging to this address must be known in your wallet, in order to send updates.
                                  The address must not be already registered, and must differ from the collateralAddress
    5. "operatorPubKey"        (string, required) The operator BLS public key. The private BLS key does not have to be known.
                                  It has to match the BLS private key which is later used when operating the masternode.
    6. "votingAddress"         (string, required) The voting key address. The private key does not have to be known by your wallet.
                                  It has to match the private key which is later used when voting on proposals.
                                  If set to an empty string, ownerAddress will be used.
    7. "payoutAddress"         (string, required) The hemis address to use for masternode reward payments.
    8. "operatorReward"        (numeric, optional) The fraction in % to share with the operator. The value must be
                                  between 0.00 and 100.00. If not set, it takes the default value of 0.0
    9. "operatorPayoutAddress" (string, optional) The address used for operator reward payments.
                                  Only allowed when the ProRegTx had a non-zero operatorReward value.
                                  If set to an empty string, the operatorAddress is used.

    Result:
    "txid"                 (string) The transaction id.
    ```

* `protx_register_fund`
    ```
    protx_register_fund "collateralAddress" "ipAndPort" "ownerAddress" "operatorPubKey" "votingAddress" "payoutAddress" (operatorReward "operatorPayoutAddress")

    Creates, funds and sends a ProTx to the network. The resulting transaction will move 10000 PIV
    to the address specified by collateralAddress and will then function as masternode collateral.

    Arguments:
    1. "collateralAddress"     (string, required) The hemis address to send the collateral to.
    2. "ipAndPort"             (string, required) IP and port in the form "IP:PORT".
                                  Must be unique on the network. Can be set to 0, which will require a ProUpServTx afterwards.
    3. "ownerAddress"          (string, required) The hemis address to use for payee updates and proposal voting.
                                  The private key belonging to this address must be known in your wallet, in order to send updates.
                                  The address must not be already registered, and must differ from the collateralAddress
    4. "operatorPubKey"        (string, required) The operator BLS public key. The private BLS key does not have to be known.
                                  It has to match the BLS private key which is later used when operating the masternode.
    5. "votingAddress"         (string, required) The voting key address. The private key does not have to be known by your wallet.
                                  It has to match the private key which is later used when voting on proposals.
                                  If set to an empty string, ownerAddress will be used.
    6. "payoutAddress"         (string, required) The hemis address to use for masternode reward payments.
    7. "operatorReward"        (numeric, optional) The fraction in % to share with the operator. The value must be
                                  between 0.00 and 100.00. If not set, it takes the default value of 0.0
    8. "operatorPayoutAddress" (string, optional) The address used for operator reward payments.
                                  Only allowed when the ProRegTx had a non-zero operatorReward value.
                                  If set to an empty string, the operatorAddress is used.

    Result:
    "txid"                        (string) The transaction id.
    ```

* `protx_register_prepare`
    ```
    protx_register_prepare "collateralHash" collateralIndex "ipAndPort" "ownerAddress" "operatorPubKey" "votingAddress" "payoutAddress" (operatorReward "operatorPayoutAddress")

    Creates an unsigned ProTx and returns it. The ProTx must be signed externally with the collateral
    key and then passed to "protx_register_submit".
    The collateral is specified through "collateralHash" and "collateralIndex" and must be an unspent transaction output.

    Arguments:
    1. "collateralHash"         (string, required) The collateral transaction hash.
    2. collateralIndex          (numeric, required) The collateral transaction output index.
    3. "ipAndPort"              (string, required) IP and port in the form "IP:PORT".
                                  Must be unique on the network. Can be set to 0, which will require a ProUpServTx afterwards.
    4. "ownerAddress"           (string, required) The hemis address to use for payee updates and proposal voting.
                                  The private key belonging to this address must be known in your wallet, in order to send updates.
                                  The address must not be already registered, and must differ from the collateralAddress
    5. "operatorPubKey"         (string, required) The operator BLS public key. The private BLS key does not have to be known.
                                  It has to match the BLS private key which is later used when operating the masternode.
    6. "votingAddress"          (string, required) The voting key address. The private key does not have to be known by your wallet.
                                  It has to match the private key which is later used when voting on proposals.
                                  If set to an empty string, ownerAddress will be used.
    7. "payoutAddress"          (string, required) The hemis address to use for masternode reward payments.
    8. "operatorReward"         (numeric, optional) The fraction in % to share with the operator. The value must be
                                  between 0.00 and 100.00. If not set, it takes the default value of 0.0
    9. "operatorPayoutAddress"  (string, optional) The address used for operator reward payments.
                                  Only allowed when the ProRegTx had a non-zero operatorReward value.
                                  If set to an empty string, the operatorAddress is used.

    Result:
    {                        (json object)
      "tx" :                 (string) The serialized ProTx in hex format.
      "collateralAddress" :  (string) The collateral address.
      "signMessage" :        (string) The string message that needs to be signed with the collateral key
    }
    ```

* `protx_register_submit`
    ```
    protx_register_submit "tx" "sig"

    Submits the specified ProTx to the network. This command will also sign the inputs of the transaction
    which were previously added by "protx_register_prepare" to cover transaction fees

    Arguments:
    1. "tx"                 (string, required) The serialized transaction previously returned by "protx_register_prepare"
    2. "sig"                (string, required) The signature signed with the collateral key. Must be in base64 format.

    Result:
    "txid"                  (string) The transaction id.
    ```

* `protx_revoke`
    ```
    protx_revoke \"proTxHash\" (\"operatorKey\" reason)\n"

    Creates and sends a ProUpRevTx to the network. This will revoke the operator key of the masternode and
    put it into the PoSe-banned state. It will also set the service field of the masternode
    to zero. Use this in case your operator key got compromised or you want to stop providing your service
    to the masternode owner.

    Arguments:
    1. "proTxHash"    (string, required) The hash of the initial ProRegTx.
    2. "operatorKey"  (string, optional) The operator BLS private key associated with the registered operator public key.
                         If not specified, or set to an empty string, then this command must be performed on the active
                         masternode with the corresponding operator key.
    3 reason          (numeric, optional) The reason for masternode service revocation. Default: 0.
                         0=not_specified, 1=service_termination, 2=compromised_keys, 3=keys_change.

    Result:
    "txid"                  (string) The transaction id.
    ```

* `protx_update_registrar`
    ```
    protx update_registrar \"proTxHash\" \"operatorPubKey\" \"votingAddress\" \"payoutAddress\" (\"ownerKey\")

    Creates and sends a ProUpRegTx to the network. This will update the operator key, voting key and payout
    address of the masternode specified by \"proTxHash\".
    The owner key of this masternode must be known to your wallet.

    Creates and sends a ProUpServTx to the network. This will update the IP address
    of a masternode, and/or the operator payout address.
    If the IP is changed for a masternode that got PoSe-banned, the ProUpServTx will also revive this masternode.

    Arguments:
    1. "proTxHash"             (string, required) The hash of the initial ProRegTx.
    2. "operatorPubKey         (string, required) The operator BLS public key. The private BLS key does not have to be known.
                                  It has to match the BLS private key which is later used when operating the masternode.
                                  If set to an empty string, the currently active operator BLS public key is reused.
    3. "votingAddress"         (string, required) The voting key address. The private key does not have to be known by your wallet.
                                  It has to match the private key which is later used when voting on proposals.
                                  If set to an empty string, the currently active voting key address is reused.
    4. "payoutAddress"         (string, required) The hemis address to use for masternode reward payments.
                                  If set to an empty string, the currently active payout address is reused.
    5. "ownerKey"              (string, optional) The owner key associated with the operator address of the masternode.
                                  If not specified, or set to an empty string, then the mn key must be known by your wallet,
                                  in order to sign the tx.
    Result:
    "txid"                        (string) The transaction id.
    ```

* `protx_update_service`
    ```
    protx_update_service "proTxHash" "ipAndPort" ("operatorPayoutAddress" "operatorKey")

    Creates and sends a ProUpServTx to the network. This will update the IP address
    of a masternode, and/or the operator payout address.
    If the IP is changed for a masternode that got PoSe-banned, the ProUpServTx will also revive this masternode.

    Arguments:
    1. "proTxHash"             (string, required) The hash of the initial ProRegTx.
    2. "ipAndPort"             (string, required) IP and port in the form "IP:PORT".
                                 If set to an empty string, the currently active ip is reused.
    3. "operatorPayoutAddress" (string, optional) The address used for operator reward payments.
                                Only allowed when the ProRegTx had a non-zero operatorReward value.
                                 If set to an empty string, the currently active one is reused.
    4. "operatorKey"           (string, optional) The operator BLS private key associated with the registered operator public key.
                                 If not specified, or set to an empty string, then this command must be performed on the active
                                 masternode with the corresponding operator key.
    Result:
    "txid"                        (string) The transaction id.
    ```

#### GUI changes

!TODO

#### Protocol changes

Starting with the enforcement block, masternode rewards and budget payments are paid as outputs of the coinbase transaction, instead of the coinstake transaction.
With this rule, a new opcode (`0xd2`) is introduced (see PR [#2275](https://github.com/hemis-Project/hemis/pull/2275)).
It enforces the same rules as the legacy cold-staking opcode, but without allowing a "free" script for the last output of the transaction.
The new opcode takes the name of `OP_CHECKCOLDSTAKEVERIFY`, and the legacy opcode (`0xd1`) is renamed to `OP_CHECKCOLDSTAKEVERIFY_LOF` (last-output-free).
Scripts with the old opcode are still accepted on the network (the restriction on the last-output is enforced after the script validation in this case), but the client creates new delegations with the new opcode, by default, after the upgrade enforcement.


Low-level RPC changes
---------------------

#### New RPC Commands

GUI changes
-----------

Build system changes
--------------------

P2P and network changes
-----------------------

Configuration changes
---------------------

Thanks to everyone who directly contributed to this release:


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.
