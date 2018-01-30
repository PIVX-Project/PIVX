##SwiftTX Technical Information

SwiftTX has been integrated into the Core Daemon in two ways:
* "push" notifications (ZMQ and `-swifttxnotify` cmd-line/config option);
* RPC commands.

####ZMQ

When a "Transaction Lock" occurs the hash of the related transaction is broadcasted through ZMQ using both the `zmqpubrawtxlock` and `zmqpubhashtxlock` channels.

* `zmqpubrawtxlock`: publishes the raw transaction when locked via SwiftTX
* `zmqpubhashtxlock`: publishes the transaction hash when locked via SwiftTX

This mechanism has been integrated into Bitcore-Node-COLX which allows for notification to be broadcast through Insight API in one of two ways:
* WebSocket: [https://github.com/COLX-Project/insight-api-colx#web-socket-api](https://github.com/COLX-Project/insight-api-colx#web-socket-api)
* API: [https://github.com/COLX-Project/insight-api-colx#swifttx-transactions](https://github.com/COLX-Project/insight-api-colx#swifttx-transactions)

####Command line option

When a wallet SwiftTX transaction is successfully locked a shell command provided in this option is executed (`%s` in `<cmd>` is replaced by TxID):

```
-swifttxnotify=<cmd>
```

####RPC

Details pertaining to an observed "Transaction Lock" can also be retrieved through RPC, it’s important however to understand the underlying mechanism.

By default, the COLX Core daemon will launch using the following constant:

```
static const int DEFAULT_SWIFTTX_DEPTH = 5;
```

This value can be overridden by passing the following argument to the COLX Core daemon:

```
-swifttxdepth=<n>
```

The key thing to understand is that this value indicates the number of "confirmations" a successful Transaction Lock represents. When Wallet RPC commands are performed (such as `listsinceblock`) this attribute is taken into account when returning information about the transaction. The value in `confirmations` field you see through RPC is showing the number of `"Blockchain Confirmations" + "SwiftTX Depth"` (assuming the funds were sent via SwiftTX).

There is also a field named `bcconfirmations`. The value in this field represents the total number of `"Blockchain Confirmations"` for a given transaction without taking into account whether it was SwiftTX or not.

**Examples**
* SwiftTX transaction just occurred:
    * confirmations: 5
    * bcconfirmations: 0
* SwiftTX transaction received one confirmation from blockchain:
    * confirmations: 6
    * bcconfirmations: 1
* non-SwiftTX transaction just occurred:
    * confirmations: 0
    * bcconfirmations: 0
* non-SwiftTX transaction received one confirmation from blockchain:
    * confirmations: 1
    * bcconfirmations: 1
