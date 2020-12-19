// Copyright (c) 2015-2020 The Zcash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "sapling/sapling_core_write.h"
#include "utilstrencodings.h"
#include "utilmoneystr.h"

static UniValue TxShieldSpendsToJSON(const CTransaction& tx) {
    UniValue vdesc(UniValue::VARR);
    if (tx.sapData) {
        for (const SpendDescription& spendDesc : tx.sapData->vShieldSpend) {
            UniValue obj(UniValue::VOBJ);
            obj.pushKV("cv", spendDesc.cv.GetHex());
            obj.pushKV("anchor", spendDesc.anchor.GetHex());
            obj.pushKV("nullifier", spendDesc.nullifier.GetHex());
            obj.pushKV("rk", spendDesc.rk.GetHex());
            obj.pushKV("proof", HexStr(spendDesc.zkproof.begin(), spendDesc.zkproof.end()));
            obj.pushKV("spendAuthSig", HexStr(spendDesc.spendAuthSig.begin(), spendDesc.spendAuthSig.end()));
            vdesc.push_back(obj);
        }
    }
    return vdesc;
}

static UniValue TxShieldOutputsToJSON(const CTransaction& tx) {
    UniValue vdesc(UniValue::VARR);
    if (tx.sapData) {
        for (const OutputDescription& outputDesc : tx.sapData->vShieldOutput) {
            UniValue obj(UniValue::VOBJ);
            obj.pushKV("cv", outputDesc.cv.GetHex());
            obj.pushKV("cmu", outputDesc.cmu.GetHex());
            obj.pushKV("ephemeralKey", outputDesc.ephemeralKey.GetHex());
            obj.pushKV("encCiphertext", HexStr(outputDesc.encCiphertext.begin(), outputDesc.encCiphertext.end()));
            obj.pushKV("outCiphertext", HexStr(outputDesc.outCiphertext.begin(), outputDesc.outCiphertext.end()));
            obj.pushKV("proof", HexStr(outputDesc.zkproof.begin(), outputDesc.zkproof.end()));
            vdesc.push_back(obj);
        }
    }
    return vdesc;
}

void TxSaplingToJSON(const CTransaction& tx, UniValue& entry) {
    if (tx.IsShieldTx()) {
        entry.pushKV("valueBalance", FormatMoney(tx.sapData->valueBalance));
        entry.pushKV("valueBalanceSat", tx.sapData->valueBalance);
        UniValue vspenddesc = TxShieldSpendsToJSON(tx);
        entry.pushKV("vShieldSpend", vspenddesc);
        UniValue voutputdesc = TxShieldOutputsToJSON(tx);
        entry.pushKV("vShieldOutput", voutputdesc);
        if (tx.sapData->hasBindingSig()) {
            entry.pushKV("bindingSig", HexStr(tx.sapData->bindingSig.begin(), tx.sapData->bindingSig.end()));
        }
    }
}
