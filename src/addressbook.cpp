// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "addressbook.h"
#include <string>

namespace AddressBook {

    namespace AddressBookPurpose {
        const std::string UNKNOWN{"unknown"};
        const std::string RECEIVE{"receive"};
        const std::string SEND{"send"};
        const std::string DELEGABLE{"delegable"};
        const std::string DELEGATOR{"delegator"};
        const std::string COLD_STAKING{"coldstaking"};
        const std::string COLD_STAKING_SEND{"coldstaking_send"};
        const std::string SHIELD_RECEIVE{"shield_receive"};
        const std::string SHIELD_SEND{"shield_spend"};
    }

    bool IsColdStakingPurpose(const std::string& purpose) {
        return purpose == AddressBookPurpose::COLD_STAKING
               || purpose == AddressBookPurpose::COLD_STAKING_SEND;
    }

    bool IsShieldPurpose(const std::string& purpose) {
        return purpose == AddressBookPurpose::SHIELD_RECEIVE
               || purpose == AddressBookPurpose::SHIELD_SEND;
    }

    bool CAddressBookData::isSendColdStakingPurpose() const {
        return purpose == AddressBookPurpose::COLD_STAKING_SEND;
    }

    bool CAddressBookData::isSendPurpose() const {
        return purpose == AddressBookPurpose::SEND;
    }

    bool CAddressBookData::isReceivePurpose() const {
        return purpose == AddressBookPurpose::RECEIVE;
    }

    bool CAddressBookData::isShieldReceivePurpose() const {
        return purpose == AddressBookPurpose::SHIELD_RECEIVE;
    }

    bool CAddressBookData::isShield() const {
        return IsShieldPurpose(purpose);
    }


}

