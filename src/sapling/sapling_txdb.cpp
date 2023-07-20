// Copyright (c) 2016-2020 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"

// Db keys
static const char DB_SAPLING_ANCHOR = 'Z';
static const char DB_SAPLING_NULLIFIER = 'S';
static const char DB_BEST_SAPLING_ANCHOR = 'z';

// Sapling
bool CCoinsViewDB::GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
    if (rt == SaplingMerkleTree::empty_root()) {
        SaplingMerkleTree new_tree;
        tree = new_tree;
        return true;
    }

    bool read = db.Read(std::make_pair(DB_SAPLING_ANCHOR, rt), tree);

    return read;
}

bool CCoinsViewDB::GetNullifier(const uint256 &nf) const {
    bool spent = false;
    return db.Read(std::make_pair(DB_SAPLING_NULLIFIER, nf), spent);
}

uint256 CCoinsViewDB::GetBestAnchor() const {
    uint256 hashBestAnchor;
    if (!db.Read(DB_BEST_SAPLING_ANCHOR, hashBestAnchor))
        return SaplingMerkleTree::empty_root();
    return hashBestAnchor;
}

void BatchWriteNullifiers(CDBBatch& batch, CNullifiersMap& mapToUse, const char& dbChar)
{
    size_t count = 0;
    size_t changed = 0;
    for (CNullifiersMap::iterator it = mapToUse.begin(); it != mapToUse.end();) {
        if (it->second.flags & CNullifiersCacheEntry::DIRTY) {
            if (!it->second.entered)
                batch.Erase(std::make_pair(dbChar, it->first));
            else
                batch.Write(std::make_pair(dbChar, it->first), true);
            changed++;
        }
        count++;
        CNullifiersMap::iterator itOld = it++;
        mapToUse.erase(itOld);
    }
    LogPrint(BCLog::COINDB, "Committed %u changed nullifiers (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
}

template<typename Map, typename MapIterator, typename MapEntry, typename Tree>
void BatchWriteAnchors(CDBBatch& batch, Map& mapToUse, const char& dbChar)
{
    size_t count = 0;
    size_t changed = 0;
    for (MapIterator it = mapToUse.begin(); it != mapToUse.end();) {
        if (it->second.flags & MapEntry::DIRTY) {
            if (!it->second.entered)
                batch.Erase(std::make_pair(dbChar, it->first));
            else {
                if (it->first != Tree::empty_root()) {
                    batch.Write(std::make_pair(dbChar, it->first), it->second.tree);
                }
            }
            changed++;
        }
        count++;
        MapIterator itOld = it++;
        mapToUse.erase(itOld);
    }
    LogPrint(BCLog::COINDB, "Committed %u changed sapling anchors (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
}

bool CCoinsViewDB::BatchWriteSapling(const uint256& hashSaplingAnchor,
                              CAnchorsSaplingMap& mapSaplingAnchors,
                              CNullifiersMap& mapSaplingNullifiers,
                              CDBBatch& batch) {

    ::BatchWriteAnchors<CAnchorsSaplingMap, CAnchorsSaplingMap::iterator, CAnchorsSaplingCacheEntry, SaplingMerkleTree>(batch, mapSaplingAnchors, DB_SAPLING_ANCHOR);
    ::BatchWriteNullifiers(batch, mapSaplingNullifiers, DB_SAPLING_NULLIFIER);
    if (!hashSaplingAnchor.IsNull())
        batch.Write(DB_BEST_SAPLING_ANCHOR, hashSaplingAnchor);
    return true;
}
