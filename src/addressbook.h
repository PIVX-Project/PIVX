// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_ADDRESSBOOK_H
#define PIVX_ADDRESSBOOK_H

#include <map>
#include <string>

namespace AddressBook {

    namespace AddressBookPurpose {
        extern const std::string UNKNOWN;
        extern const std::string RECEIVE;
        extern const std::string SEND;
        extern const std::string DELEGABLE;
        extern const std::string DELEGATOR;
        extern const std::string COLD_STAKING;
        extern const std::string COLD_STAKING_SEND;
        extern const std::string SHIELD_RECEIVE;
        extern const std::string SHIELD_SEND;
    }

    bool IsColdStakingPurpose(const std::string& purpose);
    bool IsShieldPurpose(const std::string& purpose);

/** Address book data */
    class CAddressBookData {
    public:

        std::string name;
        std::string purpose;

        CAddressBookData() {
            purpose = AddressBook::AddressBookPurpose::UNKNOWN;
        }

        typedef std::map<std::string, std::string> StringMap;
        StringMap destdata;

        bool isSendColdStakingPurpose() const;
        bool isSendPurpose() const;
        bool isReceivePurpose() const;
        bool isShieldReceivePurpose() const;
        bool isShield() const;
    };

}

#endif //PIVX_ADDRESSBOOK_H
