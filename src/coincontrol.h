// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2015-2021 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COINCONTROL_H
#define BITCOIN_COINCONTROL_H

#include "policy/feerate.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include <unordered_set>

class OutPointWrapper {
public:
    BaseOutPoint outPoint;
    CAmount value;
    bool isP2CS;

    bool operator<(const OutPointWrapper& obj2) const {
        return this->outPoint < obj2.outPoint;
    }

    bool operator==(const OutPointWrapper& obj2) const {
        return this->outPoint == obj2.outPoint;
    }
};

/** Coin Control Features. */
class CCoinControl
{
public:
    CTxDestination destChange = CNoDestination();
    //! If false, allows unselected inputs, but requires all selected inputs be used
    bool fAllowOtherInputs;
    //! Includes watch only addresses which are solvable
    bool fAllowWatchOnly;
    //! Minimum absolute fee (not per kilobyte)
    CAmount nMinimumTotalFee;
    //! Override estimated feerate
    bool fOverrideFeeRate;
    //! Feerate to use if overrideFeeRate is true
    CFeeRate nFeeRate;

    CCoinControl()
    {
        SetNull();
    }

    void SetNull()
    {
        destChange = CNoDestination();
        setSelected.clear();
        fAllowOtherInputs = false;
        fAllowWatchOnly = false;
        nMinimumTotalFee = 0;
        nFeeRate = CFeeRate(0);
        fOverrideFeeRate = false;
    }

    bool HasSelected() const
    {
        return (!setSelected.empty());
    }

    bool IsSelected(const BaseOutPoint& output) const
    {
        return (setSelected.count(OutPointWrapper{output, 0, false}) > 0);
    }

    void Select(const BaseOutPoint& output, CAmount value = 0, bool isP2CS = false)
    {
        setSelected.insert(OutPointWrapper{output, value, isP2CS});
    }

    void UnSelect(const BaseOutPoint& output)
    {
        setSelected.erase(OutPointWrapper{output, 0, false});
    }

    void UnSelectAll()
    {
        setSelected.clear();
    }

    void ListSelected(std::vector<OutPointWrapper>& vOutpoints) const
    {
        vOutpoints.assign(setSelected.begin(), setSelected.end());
    }

    unsigned int QuantitySelected() const
    {
        return setSelected.size();
    }

private:

    struct SimpleOutpointHash {
        size_t operator() (const OutPointWrapper& obj) const {
            return (UintToArith256(obj.outPoint.hash) + obj.outPoint.n).GetLow64();
        }
    };

    std::unordered_set<OutPointWrapper, SimpleOutpointHash> setSelected;
};

#endif // BITCOIN_COINCONTROL_H
