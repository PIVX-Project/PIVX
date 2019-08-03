// Copyright (c) 2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "accumulatorcheckpoints.h"
#include "accumulatorcheckpoints.json.h"

namespace AccumulatorCheckpoints
{
    std::map<int, Checkpoint> mapCheckpoints;

    UniValue read_json(const std::string& jsondata)
    {
        UniValue v;

        if (!v.read(jsondata) || !v.isArray())
        {
            return UniValue(UniValue::VARR);
        }
        return v.get_array();
    }

    bool LoadCheckpoints(const std::string& strNetwork)
    {
        UniValue v;
        if (strNetwork == "main")
            v = read_json(GetMainCheckpoints());
        else if (strNetwork == "test")
            v = read_json(GetTestCheckpoints());
        else if (strNetwork == "regtest")
            v = read_json(GetRegTestCheckpoints());
        else
            return false;

        if (v.empty())
            return false;

        for (unsigned int idx = 0; idx < v.size(); idx++) {
            const UniValue &val = v[idx];
            const UniValue &o = val.get_obj();

            const UniValue &vHeight = find_value(o, "height");
            if (!vHeight.isNum())
                return false;

            int nHeight = vHeight.get_int();
            if (nHeight < 0)
                return false;

            Checkpoint checkpoint;
            for (auto denom : libzerocoin::zerocoinDenomList) {
                const UniValue& vDenomValue = find_value(o, std::to_string(denom));
                if (!vDenomValue.isStr()) {
                    return false;
                }
                CBigNum bn = 0;
                bn.SetHex(vDenomValue.get_str());
                checkpoint.insert(std::make_pair(denom, bn));
            }

            mapCheckpoints.insert(std::make_pair(nHeight, checkpoint));
        }
        return true;
    }

    Checkpoint GetClosestCheckpoint(const int& nHeight, int& nHeightCheckpoint)
    {
        nHeightCheckpoint = -1;
        for (auto it : mapCheckpoints) {
            //only checkpoints that are less than the height requested (unless height is less than the first checkpoint)
            if (it.first < nHeight) {
                if (nHeightCheckpoint == -1)
                    nHeightCheckpoint = it.first;
                if (nHeight - it.first < nHeightCheckpoint)
                    nHeightCheckpoint = it.first;
            }
        }

        if (nHeightCheckpoint != -1)
            return mapCheckpoints.at(nHeightCheckpoint);

        return Checkpoint();
    }
}

