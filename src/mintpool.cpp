// Copyright (c) 2017 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mintpool.h"
#include "util.h"

using namespace std;

CMintPool::CMintPool()
{
    this->nCountLastGenerated = 0;
    this->nCountLastRemoved = 0;
}

CMintPool::CMintPool(uint32_t nCount)
{
    this->nCountLastRemoved = nCount;
    this->nCountLastGenerated = nCount;
}

void CMintPool::Add(const CBigNum& bnValue, const uint32_t& nCount)
{
    insert(make_pair(bnValue, nCount));
    if (nCount > nCountLastGenerated)
        nCountLastGenerated = nCount;

    LogPrintf("%s : add %s to mint pool, nCountLastGenerated=%d\n", __func__, bnValue.GetHex().substr(0, 6), nCountLastGenerated);
}

bool CMintPool::Has(const CBigNum& bnValue)
{
    return static_cast<bool>(count(bnValue));
}

std::pair<CBigNum, uint32_t> CMintPool::Get(const CBigNum& bnValue)
{
    auto it = find(bnValue);
    return *it;
}

bool SortSmallest(const pair<CBigNum, uint32_t>& a, const pair<CBigNum, uint32_t>& b)
{
    return a.second < b.second;
}

std::list<pair<CBigNum, uint32_t> > CMintPool::List()
{
    list<pair<CBigNum, uint32_t> > listMints;
    for (auto pMint : *(this)) {
        listMints.emplace_back(pMint);
    }

    listMints.sort(SortSmallest);

    return listMints;
}

void CMintPool::Reset()
{
    clear();
    nCountLastGenerated = 0;
    nCountLastRemoved = 0;
}

bool CMintPool::Front(std::pair<CBigNum, uint32_t>& pMint)
{
    if (empty())
        return false;
    pMint = *begin();
    return true;
}

bool CMintPool::Next(pair<CBigNum, uint32_t>& pMint)
{
    auto it = find(pMint.first);
    if (it == end() || ++it == end())
        return false;

    pMint = *it;
    return true;
}

void CMintPool::Remove(const CBigNum& bnValue)
{
    auto it = find(bnValue);
    if (it == end())
        return;

    LogPrintf("%s : remote %s from mint pool\n", __func__, it->first.GetHex().substr(0, 6));
    nCountLastRemoved = it->second;
    erase(it);
}



