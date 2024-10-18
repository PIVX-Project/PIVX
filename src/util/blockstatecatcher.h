// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_UTIL_BLOCKSTATECATCHER_H
#define PIVX_UTIL_BLOCKSTATECATCHER_H

#include "consensus/validation.h"
#include "validationinterface.h"

/**
 * Validation interface listener used to get feedback from ProcessNewBlock result.
 */
class BlockStateCatcher : public CValidationInterface
{
public:
    uint256 hash;
    bool found;
    CValidationState state;
    BlockStateCatcher(const uint256& hashIn) : hash(hashIn), found(false), state(){};
    void setBlockHash(const uint256& _hash) { clear(); hash = _hash; }
    void clear() { hash.SetNull(); found = false; state = CValidationState(); }
    bool stateErrorFound() { return found && state.IsError(); }

protected:
    virtual void BlockChecked(const CBlock& block, const CValidationState& stateIn) {
        if (block.GetHash() != hash) return;
        found = true;
        state = stateIn;
    };
};

class BlockStateCatcherWrapper
{
private:
    std::shared_ptr<BlockStateCatcher> stateCatcher = nullptr;
    bool isRegistered = false;

public:
    explicit BlockStateCatcherWrapper(const uint256& hashIn)
    {
        stateCatcher = std::make_shared<BlockStateCatcher>(hashIn);
    }
    ~BlockStateCatcherWrapper()
    {
        if (isRegistered) UnregisterSharedValidationInterface(stateCatcher);
    }
    void registerEvent()
    {
        RegisterSharedValidationInterface(stateCatcher);
        isRegistered = true;
    }
    BlockStateCatcher& get() const
    {
        return *stateCatcher;
    }
};

#endif // PIVX_UTIL_BLOCKSTATECATCHER_H
