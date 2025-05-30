// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evodb.h"
#include "clientversion.h"
#include "netaddress.h"

std::unique_ptr<CEvoDB> evoDb;

CEvoDBScopedCommitter::CEvoDBScopedCommitter(CEvoDB &_evoDB) :
    evoDB(_evoDB)
{
}

CEvoDBScopedCommitter::~CEvoDBScopedCommitter()
{
    if (!didCommitOrRollback)
        Rollback();
}

void CEvoDBScopedCommitter::Commit()
{
    assert(!didCommitOrRollback);
    didCommitOrRollback = true;
    evoDB.CommitCurTransaction();
}

void CEvoDBScopedCommitter::Rollback()
{
    assert(!didCommitOrRollback);
    didCommitOrRollback = true;
    evoDB.RollbackCurTransaction();
}

CEvoDB::CEvoDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(fMemory ? "" : (GetDataDir() / "evodb"), nCacheSize, fMemory, fWipe, CLIENT_VERSION | ADDRV2_FORMAT),
                                                              rootBatch(CLIENT_VERSION | ADDRV2_FORMAT),
                                                              rootDBTransaction(db, rootBatch, CLIENT_VERSION | ADDRV2_FORMAT),
                                                              curDBTransaction(rootDBTransaction, rootDBTransaction, CLIENT_VERSION | ADDRV2_FORMAT)
{
}

void CEvoDB::CommitCurTransaction()
{
    LOCK(cs);
    curDBTransaction.Commit();
}

void CEvoDB::RollbackCurTransaction()
{
    LOCK(cs);
    curDBTransaction.Clear();
}

bool CEvoDB::CommitRootTransaction()
{
    LOCK(cs);
    assert(curDBTransaction.IsClean());
    rootDBTransaction.Commit();
    bool ret = db.WriteBatch(rootBatch);
    rootBatch.Clear();
    return ret;
}

bool CEvoDB::VerifyBestBlock(const uint256& hash)
{
    uint256 hashBestBlock;
    return Read(EVODB_BEST_BLOCK, hashBestBlock) && hashBestBlock == hash;
}

void CEvoDB::WriteBestBlock(const uint256& hash)
{
    Write(EVODB_BEST_BLOCK, hash);
}
