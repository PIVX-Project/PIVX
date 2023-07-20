// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef SRC_MASTERNODECONFIG_H_
#define SRC_MASTERNODECONFIG_H_

#include "sync.h"
#include <string>
#include <vector>

class CMasternodeConfig;
extern CMasternodeConfig masternodeConfig;

class CMasternodeConfig
{
public:
    class CMasternodeEntry
    {
    private:
        std::string alias;
        std::string ip;
        std::string privKey;
        std::string txHash;
        std::string outputIndex;

    public:
        CMasternodeEntry(std::string& _alias, std::string& _ip, std::string& _privKey, std::string& _txHash, std::string& _outputIndex) :
            alias(_alias), ip(_ip), privKey(_privKey), txHash(_txHash), outputIndex(_outputIndex) { }

        const std::string& getAlias() const { return alias; }
        const std::string& getOutputIndex() const { return outputIndex; }
        bool castOutputIndex(int& n) const;
        const std::string& getPrivKey() const { return privKey; }
        const std::string& getTxHash() const { return txHash; }
        const std::string& getIp() const { return ip; }
    };

    CMasternodeConfig() { entries = std::vector<CMasternodeEntry>(); }

    void clear() { LOCK(cs_entries); entries.clear(); }
    bool read(std::string& strErr);
    CMasternodeConfig::CMasternodeEntry* add(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex);
    void remove(std::string alias);

    std::vector<CMasternodeEntry> getEntries() { LOCK(cs_entries); return entries; }

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
    std::vector<CMasternodeEntry> entries;
    Mutex cs_entries;
};


#endif /* SRC_MASTERNODECONFIG_H_ */
