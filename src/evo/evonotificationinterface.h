// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_EVO_EVONOTIFICATIONINTERFACE_H
#define PIVX_EVO_EVONOTIFICATIONINTERFACE_H

#include "validationinterface.h"

class EvoNotificationInterface : public CValidationInterface
{
public:
    virtual ~EvoNotificationInterface() = default;

    // a small helper to initialize current block height in sub-modules on startup
    void InitializeCurrentBlockTip();

protected:
    // CValidationInterface
    void AcceptedBlockHeader(const CBlockIndex* pindexNew) override;
    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;
    void NotifyMasternodeListChanged(bool undo, const CDeterministicMNList& oldMNList, const CDeterministicMNListDiff& diff) override;
};

#endif // PIVX_EVO_EVONOTIFICATIONINTERFACE_H
