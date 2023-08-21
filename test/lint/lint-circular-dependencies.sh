#!/usr/bin/env bash
#
# Copyright (c) 2018-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Check for circular dependencies

export LC_ALL=C

EXPECTED_CIRCULAR_DEPENDENCIES=(
    "activegamemaster -> gamemasterman -> activegamemaster"
    "budget/budgetmanager -> validation -> budget/budgetmanager"
    "chain -> legacy/stakemodifier -> chain"
    "chainparamsbase -> util/system -> chainparamsbase"
    "consensus/params -> consensus/upgrades -> consensus/params"
    "evo/deterministicgms -> gamemasterman -> evo/deterministicgms"
    "evo/specialtx_validation -> llmq/quorums_blockprocessor -> evo/specialtx_validation"
    "evo/specialtx_validation -> validation -> evo/specialtx_validation"
    "kernel -> validation -> kernel"
    "gamemaster -> gamemasterman -> gamemaster"
    "gamemaster-payments -> gamemasterman -> gamemaster-payments"
    "gamemaster-payments -> validation -> gamemaster-payments"
    "gamemaster-sync -> gamemasterman -> gamemaster-sync"
    "gamemasterman -> validation -> gamemasterman"
    "net -> tiertwo/net_gamemasters -> net"
    "policy/fees -> txmempool -> policy/fees"
    "policy/policy -> validation -> policy/policy"
    "qt/hemis/hemisgui -> qt/hemis/qtutils -> qt/hemis/hemisgui"
    "qt/hemis/qtutils -> qt/hemis/snackbar -> qt/hemis/qtutils"
    "sapling/saplingscriptpubkeyman -> wallet/wallet -> sapling/saplingscriptpubkeyman"
    "spork -> sporkdb -> spork"
    "spork -> validation -> spork"
    "txmempool -> validation -> txmempool"
    "validation -> validationinterface -> validation"
    "validation -> zpiv/zpivmodule -> validation"
    "wallet/fees -> wallet/wallet -> wallet/fees"
    "wallet/scriptpubkeyman -> wallet/wallet -> wallet/scriptpubkeyman"
    "wallet/wallet -> wallet/walletdb -> wallet/wallet"
    "chain -> legacy/stakemodifier -> stakeinput -> chain"
    "chain -> legacy/stakemodifier -> validation -> chain"
    "legacy/validation_zerocoin_legacy -> wallet/wallet -> validation -> legacy/validation_zerocoin_legacy"
    "llmq/quorums -> llmq/quorums_connections -> llmq/quorums"
    "llmq/quorums_dkgsession -> llmq/quorums_dkgsessionmgr -> llmq/quorums_dkgsessionhandler -> llmq/quorums_dkgsession"
    "llmq/quorums_dkgsessionhandler -> net_processing -> llmq/quorums_dkgsessionmgr -> llmq/quorums_dkgsessionhandler"
    "llmq/quorums_signing -> net_processing -> llmq/quorums_signing"
    "llmq/quorums -> llmq/quorums_connections -> tiertwo/net_gamemasters -> llmq/quorums"
    "llmq/quorums_chainlocks -> net_processing -> llmq/quorums_chainlocks"
    "llmq/quorums_chainlocks -> validation -> llmq/quorums_chainlocks"
    "activegamemaster -> tiertwo/net_gamemasters -> llmq/quorums -> activegamemaster"
    "chain -> legacy/stakemodifier -> validation -> validationinterface -> chain"
    "chain -> legacy/stakemodifier -> stakeinput -> txdb -> chain"
    "chain -> legacy/stakemodifier -> validation -> checkpoints -> chain"
    "chain -> legacy/stakemodifier -> validation -> undo -> chain"
    "chain -> legacy/stakemodifier -> validation -> pow -> chain"
    "evo/deterministicgms -> gamemasterman -> net -> tiertwo/net_gamemasters -> evo/deterministicgms"
    "evo/deterministicgms -> gamemasterman -> validation -> validationinterface -> evo/deterministicgms"
)

EXIT_CODE=0

CIRCULAR_DEPENDENCIES=()

IFS=$'\n'
for CIRC in $(cd src && ../contrib/devtools/circular-dependencies.py {*,*/*,*/*/*}.{h,cpp} | sed -e 's/^Circular dependency: //'); do
    CIRCULAR_DEPENDENCIES+=( "$CIRC" )
    IS_EXPECTED_CIRC=0
    for EXPECTED_CIRC in "${EXPECTED_CIRCULAR_DEPENDENCIES[@]}"; do
        if [[ "${CIRC}" == "${EXPECTED_CIRC}" ]]; then
            IS_EXPECTED_CIRC=1
            break
        fi
    done
    if [[ ${IS_EXPECTED_CIRC} == 0 ]]; then
        echo "A new circular dependency in the form of \"${CIRC}\" appears to have been introduced."
        echo
        EXIT_CODE=1
    fi
done

for EXPECTED_CIRC in "${EXPECTED_CIRCULAR_DEPENDENCIES[@]}"; do
    IS_PRESENT_EXPECTED_CIRC=0
    for CIRC in "${CIRCULAR_DEPENDENCIES[@]}"; do
        if [[ "${CIRC}" == "${EXPECTED_CIRC}" ]]; then
            IS_PRESENT_EXPECTED_CIRC=1
            break
        fi
    done
    if [[ ${IS_PRESENT_EXPECTED_CIRC} == 0 ]]; then
        echo "Good job! The circular dependency \"${EXPECTED_CIRC}\" is no longer present."
        echo "Please remove it from EXPECTED_CIRCULAR_DEPENDENCIES in $0"
        echo "to make sure this circular dependency is not accidentally reintroduced."
        echo
        EXIT_CODE=1
    fi
done

exit ${EXIT_CODE}
