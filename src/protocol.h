// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Copyright (c) 2014-2021 The Dash Core developers
// Copyright (c) 2016-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __cplusplus
#error This header can only be compiled as C++.
#endif

#ifndef BITCOIN_PROTOCOL_H
#define BITCOIN_PROTOCOL_H

#include "netaddress.h"
#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "version.h"

#include <stdint.h>
#include <string>

/** Message header.
 * (4) message start.
 * (12) command.
 * (4) size.
 * (4) checksum.
 */
class CMessageHeader
{
public:
    static constexpr size_t MESSAGE_START_SIZE = 4;
    static constexpr size_t COMMAND_SIZE = 12;
    static constexpr size_t MESSAGE_SIZE_SIZE = 4;
    static constexpr size_t CHECKSUM_SIZE = 4;
    static constexpr size_t MESSAGE_SIZE_OFFSET = MESSAGE_START_SIZE + COMMAND_SIZE;
    static constexpr size_t CHECKSUM_OFFSET = MESSAGE_SIZE_OFFSET + MESSAGE_SIZE_SIZE;
    static constexpr size_t HEADER_SIZE = MESSAGE_START_SIZE + COMMAND_SIZE + MESSAGE_SIZE_SIZE + CHECKSUM_SIZE;
    typedef unsigned char MessageStartChars[MESSAGE_START_SIZE];

    explicit CMessageHeader(const MessageStartChars& pchMessageStartIn);

    /** Construct a P2P message header from message-start characters, a command and the size of the message.
     * @note Passing in a `pszCommand` longer than COMMAND_SIZE will result in a run-time assertion error.
     */
    CMessageHeader(const MessageStartChars& pchMessageStartIn, const char* pszCommand, unsigned int nMessageSizeIn);

    std::string GetCommand() const;
    bool IsValid(const MessageStartChars& messageStart) const;

    SERIALIZE_METHODS(CMessageHeader, obj) { READWRITE(obj.pchMessageStart, obj.pchCommand, obj.nMessageSize, obj.pchChecksum); }

    char pchMessageStart[MESSAGE_START_SIZE];
    char pchCommand[COMMAND_SIZE];
    uint32_t nMessageSize;
    uint8_t pchChecksum[CHECKSUM_SIZE];
};

/**
 * Bitcoin protocol message types. When adding new message types, don't forget
 * to update allNetMessageTypes in protocol.cpp.
 */
namespace NetMsgType
{
/**
 * The version message provides information about the transmitting node to the
 * receiving node at the beginning of a connection.
 * @see https://bitcoin.org/en/developer-reference#version
 */
extern const char* VERSION;
/**
 * The verack message acknowledges a previously-received version message,
 * informing the connecting node that it can begin to send other messages.
 * @see https://bitcoin.org/en/developer-reference#verack
 */
extern const char* VERACK;
/**
 * The addr (IP address) message relays connection information for peers on the
 * network.
 * @see https://bitcoin.org/en/developer-reference#addr
 */
extern const char* ADDR;
/**
 * The addrv2 message relays connection information for peers on the network just
 * like the addr message, but is extended to allow gossiping of longer node
 * addresses (see BIP155).
 */
extern const char *ADDRV2;
/**
 * The sendaddrv2 message signals support for receiving ADDRV2 messages (BIP155).
 * It also implies that its sender can encode as ADDRV2 and would send ADDRV2
 * instead of ADDR to a peer that has signaled ADDRV2 support by sending SENDADDRV2.
 */
extern const char *SENDADDRV2;
/**
 * The inv message (inventory message) transmits one or more inventories of
 * objects known to the transmitting peer.
 * @see https://bitcoin.org/en/developer-reference#inv
 */
extern const char* INV;
/**
 * The getdata message requests one or more data objects from another node.
 * @see https://bitcoin.org/en/developer-reference#getdata
 */
extern const char* GETDATA;
/**
 * The merkleblock message is a reply to a getdata message which requested a
 * block using the inventory type MSG_MERKLEBLOCK.
 * @since protocol version 70001 as described by BIP37.
 * @see https://bitcoin.org/en/developer-reference#merkleblock
 */
extern const char* MERKLEBLOCK;
/**
 * The getblocks message requests an inv message that provides block header
 * hashes starting from a particular point in the block chain.
 * @see https://bitcoin.org/en/developer-reference#getblocks
 */
extern const char* GETBLOCKS;
/**
 * The getheaders message requests a headers message that provides block
 * headers starting from a particular point in the block chain.
 * @since protocol version 31800.
 * @see https://bitcoin.org/en/developer-reference#getheaders
 */
extern const char* GETHEADERS;
/**
 * The tx message transmits a single transaction.
 * @see https://bitcoin.org/en/developer-reference#tx
 */
extern const char* TX;
/**
 * The headers message sends one or more block headers to a node which
 * previously requested certain headers with a getheaders message.
 * @since protocol version 31800.
 * @see https://bitcoin.org/en/developer-reference#headers
 */
extern const char* HEADERS;
/**
 * The block message transmits a single serialized block.
 * @see https://bitcoin.org/en/developer-reference#block
 */
extern const char* BLOCK;
/**
 * The getaddr message requests an addr message from the receiving node,
 * preferably one with lots of IP addresses of other receiving nodes.
 * @see https://bitcoin.org/en/developer-reference#getaddr
 */
extern const char* GETADDR;
/**
 * The mempool message requests the TXIDs of transactions that the receiving
 * node has verified as valid but which have not yet appeared in a block.
 * @since protocol version 60002.
 * @see https://bitcoin.org/en/developer-reference#mempool
 */
extern const char* MEMPOOL;
/**
 * The ping message is sent periodically to help confirm that the receiving
 * peer is still connected.
 * @see https://bitcoin.org/en/developer-reference#ping
 */
extern const char* PING;
/**
 * The pong message replies to a ping message, proving to the pinging node that
 * the ponging node is still alive.
 * @since protocol version 60001 as described by BIP31.
 * @see https://bitcoin.org/en/developer-reference#pong
 */
extern const char* PONG;
/**
 * The alert message warns nodes of problems that may affect them or the rest
 * of the network.
 * @since protocol version 311.
 * @see https://bitcoin.org/en/developer-reference#alert
 */
extern const char* ALERT;
/**
 * The notfound message is a reply to a getdata message which requested an
 * object the receiving node does not have available for relay.
 * @ince protocol version 70001.
 * @see https://bitcoin.org/en/developer-reference#notfound
 */
extern const char* NOTFOUND;
/**
 * The filterload message tells the receiving peer to filter all relayed
 * transactions and requested merkle blocks through the provided filter.
 * @since protocol version 70001 as described by BIP37.
 *   Only available with service bit NODE_BLOOM since protocol version
 *   70011 as described by BIP111.
 * @see https://bitcoin.org/en/developer-reference#filterload
 */
extern const char* FILTERLOAD;
/**
 * The filteradd message tells the receiving peer to add a single element to a
 * previously-set bloom filter, such as a new public key.
 * @since protocol version 70001 as described by BIP37.
 *   Only available with service bit NODE_BLOOM since protocol version
 *   70011 as described by BIP111.
 * @see https://bitcoin.org/en/developer-reference#filteradd
 */
extern const char* FILTERADD;
/**
 * The filterclear message tells the receiving peer to remove a previously-set
 * bloom filter.
 * @since protocol version 70001 as described by BIP37.
 *   Only available with service bit NODE_BLOOM since protocol version
 *   70011 as described by BIP111.
 * @see https://bitcoin.org/en/developer-reference#filterclear
 */
extern const char* FILTERCLEAR;
/**
 * Indicates that a node prefers to receive new block announcements via a
 * "headers" message rather than an "inv".
 * @since protocol version 70012 as described by BIP130.
 * @see https://bitcoin.org/en/developer-reference#sendheaders
 */
extern const char* SENDHEADERS;
/**
 * The spork message is used to send spork values to connected
 * peers
 */
extern const char* SPORK;
/**
 * The getsporks message is used to request spork data from connected peers
 */
extern const char* GETSPORKS;
/**
 * The gmbroadcast message is used to broadcast gamemaster startup data to connected peers
 */
extern const char* GMBROADCAST;
/**
 * The gmbroadcast2 message is used to broadcast gamemaster startup data to connected peers
 * Supporting BIP155 node addresses.
 */
extern const char* GMBROADCAST2;
/**
 * The gmping message is used to ensure a gamemaster is still active
 */
extern const char* GMPING;
/**
 * The gmwinner message is used to relay and distribute consensus for gamemaster
 * payout ordering
 */
extern const char* GMWINNER;
/**
 * The getgmwinners message is used to request winning gamemaster data from connected peers
 */
extern const char* GETGMWINNERS;
/**
* The dseg message is used to request the Gamemaster list or an specific entry
*/
extern const char* GETGMLIST;
/**
 * The budgetproposal message is used to broadcast or relay budget proposal metadata to connected peers
 */
extern const char* BUDGETPROPOSAL;
/**
 * The budgetvote message is used to broadcast or relay budget proposal votes to connected peers
 */
extern const char* BUDGETVOTE;
/**
 * The budgetvotesync message is used to request budget vote data from connected peers
 */
extern const char* BUDGETVOTESYNC;
/**
 * The finalbudget message is used to broadcast or relay finalized budget metadata to connected peers
 */
extern const char* FINALBUDGET;
/**
 * The finalbudgetvote message is used to broadcast or relay finalized budget votes to connected peers
 */
extern const char* FINALBUDGETVOTE;
/**
 * The syncstatuscount message is used to track the layer 2 syncing process
 */
extern const char* SYNCSTATUSCOUNT;
/**
 * The qfcommit message is used to propagate LLMQ final commitments
 */
extern const char* QFCOMMITMENT;
/**
 * The qsendrecsigs message is used to propagate LLMQ intra-quorum partial recovered signatures
 */
extern const char* QSENDRECSIGS;
/**
 * The gmauth message is used authenticate GM connections
 */
extern const char* GMAUTH;
/*
 * Messages for LLMQ-DKG inter-quorum communication
 */
extern const char* QCONTRIB;
extern const char* QCOMPLAINT;
extern const char* QJUSTIFICATION;
extern const char* QPCOMMITMENT;
extern const char* QSIGSHARESINV;
extern const char* QGETSIGSHARES;
extern const char* QBSIGSHARES;
extern const char* QSIGREC;
extern const char* CLSIG;
}; // namespace NetMsgType

/* Get a vector of all valid message types (see above) */
const std::vector<std::string>& getAllNetMessageTypes();

/* Get a vector of all tier two valid message types (see above) */
const std::vector<std::string>& getTierTwoNetMessageTypes();

/** nServices flags */
enum ServiceFlags : uint64_t {
    // Nothing
    NODE_NONE = 0,
    // NODE_NETWORK means that the node is capable of serving the block chain. It is currently
    // set by all Bitcoin Core nodes, and is unset by SPV clients or other peers that just want
    // network services but don't provide them.
    NODE_NETWORK = (1 << 0),

    // NODE_BLOOM means the node is capable and willing to handle bloom-filtered connections.
    NODE_BLOOM = (1 << 2),

    // NODE_BLOOM_WITHOUT_GM means the node has the same features as NODE_BLOOM with the only difference
    // that the node doesn't want to receive master nodes messages. (the 1<<3 was not picked as constant because on bitcoin 0.14 is witness and we want that update here )
    NODE_BLOOM_WITHOUT_GM = (1 << 4),

    // Bits 24-31 are reserved for temporary experiments. Just pick a bit that
    // isn't getting used, or one not being used much, and notify the
    // bitcoin-development mailing list. Remember that service bits are just
    // unauthenticated advertisements, so your code must be robust against
    // collisions and other cases where nodes may be advertising a service they
    // do not actually support. Other service bits should be allocated via the
    // BIP process.
};

/** A CService with information about it as peer */
class CAddress : public CService
{
    static constexpr uint32_t TIME_INIT{100000000};

    /** Historically, CAddress disk serialization stored the CLIENT_VERSION, optionally OR'ed with
     *  the ADDRV2_FORMAT flag to indicate V2 serialization. The first field has since been
     *  disentangled from client versioning, and now instead:
     *  - The low bits (masked by DISK_VERSION_IGNORE_MASK) store the fixed value DISK_VERSION_INIT,
     *    (in case any code exists that treats it as a client version) but are ignored on
     *    deserialization.
     *  - The high bits (masked by ~DISK_VERSION_IGNORE_MASK) store actual serialization information.
     *    Only 0 or DISK_VERSION_ADDRV2 (equal to the historical ADDRV2_FORMAT) are valid now, and
     *    any other value triggers a deserialization failure. Other values can be added later if
     *    needed.
     *
     *  For disk deserialization, ADDRV2_FORMAT in the stream version signals that ADDRV2
     *  deserialization is permitted, but the actual format is determined by the high bits in the
     *  stored version field. For network serialization, the stream version having ADDRV2_FORMAT or
     *  not determines the actual format used (as it has no embedded version number).
     */
    static constexpr uint32_t DISK_VERSION_INIT{220000};
    static constexpr uint32_t DISK_VERSION_IGNORE_MASK{0b00000000'00000111'11111111'11111111};
    /** The version number written in disk serialized addresses to indicate V2 serializations.
     * It must be exactly 1<<29, as that is the value that historical versions used for this
     * (they used their internal ADDRV2_FORMAT flag here). */
    static constexpr uint32_t DISK_VERSION_ADDRV2{1 << 29};
    static_assert((DISK_VERSION_INIT & ~DISK_VERSION_IGNORE_MASK) == 0, "DISK_VERSION_INIT must be covered by DISK_VERSION_IGNORE_MASK");
    static_assert((DISK_VERSION_ADDRV2 & DISK_VERSION_IGNORE_MASK) == 0, "DISK_VERSION_ADDRV2 must not be covered by DISK_VERSION_IGNORE_MASK");

public:
    CAddress() : CService{} {};
    CAddress(CService ipIn, ServiceFlags nServicesIn) : CService{ipIn}, nServices{nServicesIn} {};
    CAddress(CService ipIn, ServiceFlags nServicesIn, uint32_t nTimeIn) : CService{ipIn}, nTime{nTimeIn}, nServices{nServicesIn} {};

    SERIALIZE_METHODS(CAddress, obj)
    {
        // CAddress has a distinct network serialization and a disk serialization, but it should never
        // be hashed (except through CHashWriter in addrdb.cpp, which sets SER_DISK), and it's
        // ambiguous what that would mean. Make sure no code relying on that is introduced:
        assert(!(s.GetType() & SER_GETHASH));
        bool use_v2;
        bool store_time;
        if (s.GetType() & SER_DISK) {
            // In the disk serialization format, the encoding (v1 or v2) is determined by a flag version
            // that's part of the serialization itself. ADDRV2_FORMAT in the stream version only determines
            // whether V2 is chosen/permitted at all.
            uint32_t stored_format_version = DISK_VERSION_INIT;
            if (s.GetVersion() & ADDRV2_FORMAT) stored_format_version |= DISK_VERSION_ADDRV2;
            READWRITE(stored_format_version);
            stored_format_version &= ~DISK_VERSION_IGNORE_MASK; // ignore low bits
            if (stored_format_version == 0) {
                use_v2 = false;
            } else if (stored_format_version == DISK_VERSION_ADDRV2 && (s.GetVersion() & ADDRV2_FORMAT)) {
                // Only support v2 deserialization if ADDRV2_FORMAT is set.
                use_v2 = true;
            } else {
                throw std::ios_base::failure("Unsupported CAddress disk format version");
            }
            store_time = true;
        } else {
            // In the network serialization format, the encoding (v1 or v2) is determined directly by
            // the value of ADDRV2_FORMAT in the stream version, as no explicitly encoded version
            // exists in the stream.
            assert(s.GetType() & SER_NETWORK);
            use_v2 = s.GetVersion() & ADDRV2_FORMAT;
            // The only time we serialize a CAddress object without nTime is in
            // the initial VERSION messages which contain two CAddress records.
            // At that point, the serialization version is INIT_PROTO_VERSION.
            // After the version handshake, serialization version is >=
            // MIN_PEER_PROTO_VERSION and all ADDR messages are serialized with
            // nTime.
            store_time = s.GetVersion() != INIT_PROTO_VERSION;
        }

        SER_READ(obj, obj.nTime = TIME_INIT);
        if (store_time) READWRITE(obj.nTime);
        // nServices is serialized as CompactSize in V2; as uint64_t in V1.
        if (use_v2) {
            uint64_t services_tmp;
            SER_WRITE(obj, services_tmp = obj.nServices);
            READWRITE(Using<CompactSizeFormatter<false>>(services_tmp));
            SER_READ(obj, obj.nServices = static_cast<ServiceFlags>(services_tmp));
        } else {
            READWRITE(Using<CustomUintFormatter<8>>(obj.nServices));
        }
        // Invoke V1/V2 serializer for CService parent object.
        OverrideStream<Stream> os(&s, s.GetType(), use_v2 ? ADDRV2_FORMAT : 0);
        SerReadWriteMany(os, ser_action, ReadWriteAsHelper<CService>(obj));
    }

    //! Always included in serialization, except in the network format on INIT_PROTO_VERSION.
    uint32_t nTime{TIME_INIT};
    //! Serialized as uint64_t in V1, and as CompactSize in V2.
    ServiceFlags nServices{NODE_NONE};
};

/** getdata message types */
enum GetDataMsg {
    UNDEFINED = 0,
    MSG_TX,
    MSG_BLOCK,
    // Nodes may always request a MSG_FILTERED_BLOCK in a getdata, however,
    // MSG_FILTERED_BLOCK should not appear in any invs except as a part of getdata.
    MSG_FILTERED_BLOCK,
    MSG_TXLOCK_REQUEST, // Deprecated
    MSG_TXLOCK_VOTE,    // Deprecated
    MSG_SPORK,
    MSG_GAMEMASTER_WINNER,
    MSG_GAMEMASTER_SCANNING_ERROR,
    MSG_BUDGET_VOTE,
    MSG_BUDGET_PROPOSAL,
    MSG_BUDGET_FINALIZED,
    MSG_BUDGET_FINALIZED_VOTE,
    MSG_GAMEMASTER_QUORUM,
    MSG_GAMEMASTER_ANNOUNCE,
    MSG_GAMEMASTER_PING,
    MSG_DSTX, // Deprecated
    MSG_QUORUM_FINAL_COMMITMENT,
    MSG_QUORUM_CONTRIB,
    MSG_QUORUM_COMPLAINT,
    MSG_QUORUM_JUSTIFICATION,
    MSG_QUORUM_PREMATURE_COMMITMENT,
    MSG_QUORUM_RECOVERED_SIG,
    MSG_CLSIG,
    MSG_TYPE_MAX = MSG_CLSIG,
};

/** inv message data */
class CInv
{
public:
    CInv();
    CInv(int typeIn, const uint256& hashIn);

    SERIALIZE_METHODS(CInv, obj) { READWRITE(obj.type, obj.hash); }

    friend bool operator<(const CInv& a, const CInv& b);

    bool IsGameMasterType() const;
    std::string ToString() const;

    // TODO: make private (improve encapsulation)
    int type;
    uint256 hash;

private:
    std::string GetCommand() const;
};

#endif // BITCOIN_PROTOCOL_H
