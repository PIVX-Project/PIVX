// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef SRC_GAMEMASTERCONFIG_H_
#define SRC_GAMEMASTERCONFIG_H_

#include "sync.h"
#include <string>
#include <vector>

class CGamemasterConfig;
extern CGamemasterConfig gamemasterConfig;

class CGamemasterConfig
{
public:
    class CGamemasterEntry
    {
    private:
        std::string alias;
        std::string ip;
        std::string privKey;
        std::string txHash;
        std::string outputIndex;

    public:
        CGamemasterEntry(std::string& _alias, std::string& _ip, std::string& _privKey, std::string& _txHash, std::string& _outputIndex) :
            alias(_alias), ip(_ip), privKey(_privKey), txHash(_txHash), outputIndex(_outputIndex) { }

        const std::string& getAlias() const { return alias; }
        const std::string& getOutputIndex() const { return outputIndex; }
        bool castOutputIndex(int& n) const;
        const std::string& getPrivKey() const { return privKey; }
        const std::string& getTxHash() const { return txHash; }
        const std::string& getIp() const { return ip; }
    };

    CGamemasterConfig() { entries = std::vector<CGamemasterEntry>(); }

    void clear() { LOCK(cs_entries); entries.clear(); }
    bool read(std::string& strErr);
    CGamemasterConfig::CGamemasterEntry* add(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex);
    void remove(std::string alias);

    std::vector<CGamemasterEntry> getEntries() { LOCK(cs_entries); return entries; }

    int getCount()
    {
        LOCK(cs_entries);
        int c = -1;
        for (const auto& e : entries) {
            if (!e.getAlias().empty()) c++;
        }
        return c;
    }

private:
    std::vector<CGamemasterEntry> entries;
    Mutex cs_entries;
};


#endif /* SRC_GAMEMASTERCONFIG_H_ */
