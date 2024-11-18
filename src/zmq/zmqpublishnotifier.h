// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_ZMQ_ZMQPUBLISHNOTIFIER_H
#define PIVX_ZMQ_ZMQPUBLISHNOTIFIER_H

#include "zmqabstractnotifier.h"

class CBlockIndex;

class CZMQAbstractPublishNotifier : public CZMQAbstractNotifier
{
private:
    uint32_t nSequence{0}; // upcounting per message sequence number

public:

    /* send zmq multipart message
       parts:
          * command
          * data
          * message sequence number
    */
    bool SendMessage(const char *command, const void* data, size_t size);

    bool Initialize(void *pcontext);
    void Shutdown();
};

class CZMQPublishHashBlockNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyBlock(const CBlockIndex *pindex);
};

class CZMQPublishHashChainLockNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyChainLock(const CBlockIndex *pindex, const llmq::CChainLockSig& clsig) override;
};

class CZMQPublishHashTransactionNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyTransaction(const CTransaction &transaction);
};

class CZMQPublishRawBlockNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyBlock(const CBlockIndex *pindex);
};

class CZMQPublishRawChainLockNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyChainLock(const CBlockIndex *pindex, const llmq::CChainLockSig& clsig) override;
};

class CZMQPublishRawChainLockSigNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyChainLock(const CBlockIndex *pindex, const llmq::CChainLockSig& clsig) override;
};

class CZMQPublishRawTransactionNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyTransaction(const CTransaction &transaction);
};

#endif // PIVX_ZMQ_ZMQPUBLISHNOTIFIER_H
