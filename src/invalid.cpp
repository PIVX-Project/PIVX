// Copyright (c) 2018-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "invalid.h"
#include "invalid_outpoints.json.h"

#include "primitives/transaction.h"

#include <univalue.h>

namespace invalid_out
{
    std::set<COutPoint> setInvalidOutPoints;

    UniValue read_json(const std::string& jsondata)
    {
        UniValue v;

        if (!v.read(jsondata) || !v.isArray())
        {
            return UniValue(UniValue::VARR);
        }
        return v.get_array();
    }

    bool LoadOutpoints()
    {
        UniValue v = read_json(LoadInvalidOutPoints());

        if (v.empty())
            return false;

        for (unsigned int idx = 0; idx < v.size(); idx++) {
            const UniValue &val = v[idx];
            const UniValue &o = val.get_obj();

            const UniValue &vTxid = find_value(o, "txid");
            if (!vTxid.isStr())
                return false;

            uint256 txid = uint256S(vTxid.get_str());
            if (txid.IsNull())
                return false;

            const UniValue &vN = find_value(o, "n");
            if (!vN.isNum())
                return false;

            auto n = static_cast<uint32_t>(vN.get_int());
            COutPoint out(txid, n);
            setInvalidOutPoints.insert(out);
        }
        return true;
    }

    bool ContainsOutPoint(const COutPoint& out)
    {
        return static_cast<bool>(setInvalidOutPoints.count(out));
    }
}

