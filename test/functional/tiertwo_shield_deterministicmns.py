#!/usr/bin/env python3
# Copyright (c) 2023 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test shield deterministic masternodes"""

from decimal import Decimal
from random import randrange
import time

from tiertwo_deterministicmns import DIP3Test
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    is_coin_locked_by,
)


class DIP3ShieldTest(DIP3Test):

    def run_test(self):
        self.disable_mocktime()

        # Additional connections to miner and owner
        for nodePos in [self.minerPos, self.controllerPos]:
            self.connect_to_all(nodePos)
        miner = self.nodes[self.minerPos]
        controller = self.nodes[self.controllerPos]

        # Enforce mn payments and reject legacy mns at block 131
        self.activate_spork(0, "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT")
        assert_equal("success", self.set_spork(self.minerPos, "SPORK_21_LEGACY_MNS_MAX_HEIGHT", 130))
        time.sleep(1)
        assert_equal([130] * self.num_nodes, [self.get_spork(x, "SPORK_21_LEGACY_MNS_MAX_HEIGHT")
                                              for x in range(self.num_nodes)])
        mns = []

        # Mine 100 blocks
        self.log.info("Mining...")
        miner.generate(110)
        self.sync_blocks()
        self.assert_equal_for_all(110, "getblockcount")

        # DIP3 activates at block 130.
        miner.generate(130 - miner.getblockcount())
        self.sync_blocks()
        self.assert_equal_for_all(130, "getblockcount")

        # -- DIP3 enforced and SPORK_21 active here --
        self.wait_until_mnsync_completed()

        # enabled/total masternodes: 0/0
        self.check_mn_enabled_count(0, 0)

        # Create 2 SDMNs:
        self.log.info("Initializing masternodes...")
        self.add_new_dmn(mns, "internal", False)
        self.add_new_dmn(mns, "internal", False)

        for mn in mns:
            self.nodes[mn.idx].initmasternode(mn.operator_sk)
            time.sleep(1)
        miner.generate(1)
        self.sync_blocks()

        # enabled/total masternodes: 2/2
        self.check_mn_enabled_count(2, 2)
        self.log.info("Masternodes started.")
        self.check_mn_list(mns)

        # Restart the controller and check that the collaterals are still locked
        self.log.info("Restarting controller...")
        self.restart_controller()
        time.sleep(1)
        for mn in mns:
            if not is_coin_locked_by(controller, mn.collateral, mn.transparent):
                raise Exception(
                    "Collateral %s of mn with idx=%d is not locked" % (mn.collateral, mn.idx)
                )
        self.log.info("Collaterals still locked.")

        # Test collateral spending of a SDMN
        s_dmn = mns.pop(randrange(len(mns)))  # pop one at random
        self.log.info("Spending collateral of mn with idx=%d..." % s_dmn.idx)

        # Try to spend the shield collateral
        shield_collateral = {"txid": "%064x" % s_dmn.collateral.hash, "vout": s_dmn.collateral.n}
        controller.lockunspent(True, False, [shield_collateral])

        # Sanity check on the controller notes: the collateral of the other sdmn is still locked
        assert_equal(len(controller.listlockunspent()["shielded"]), 1)
        assert_equal(controller.getsaplingnotescount(), 2)

        recipient2 = [{"address": controller.getnewshieldaddress(), "amount": Decimal('90')}]
        controller.shieldsendmany("from_shield", recipient2)
        self.sync_mempools([miner, controller])
        miner.generate(2)
        self.sync_all()

        # enabled/total masternodes: 1/1
        self.check_mn_enabled_count(1, 1)
        self.check_mn_list(mns)

        s_dmn_keys = [s_dmn.operator_pk, s_dmn.operator_sk]

        # Try to register s_dmn with the already spent shield collateral
        self.log.info("Trying using a spent note...")
        assert_raises_rpc_error(-1, "bad-txns-shielded-requirements-not-met",
                                self.register_new_dmn, s_dmn.idx, self.minerPos, self.controllerPos, "internal", False, outpoint=s_dmn.collateral, op_blskeys=s_dmn_keys)

        # Register s_dmn again, with the collateral of s_dmn2
        # s_dmn must be added again to the list, and s_dmn2 must be removed
        s_dmn2 = mns.pop(0)
        s_dmn2_keys = [s_dmn2.operator_pk, s_dmn2.operator_sk]
        self.log.info("Reactivating node %d reusing the collateral of node %d..." % (s_dmn.idx, s_dmn2.idx))
        mns.append(self.register_new_dmn(s_dmn.idx, self.minerPos, self.controllerPos, "internal", False,
                                         outpoint=s_dmn2.collateral, op_blskeys=s_dmn_keys))
        miner.generate(1)
        self.sync_blocks()

        # enabled/total masternodes: 1/1
        self.check_mn_enabled_count(1, 1)
        self.check_mn_list(mns)

        # Now try to register dmn2 again with an already-used IP
        self.log.info("Trying duplicate IP...")
        rand_idx = mns[randrange(len(mns))].idx
        assert_raises_rpc_error(-1, "bad-protx-dup-IP-address",
                                self.register_new_dmn, rand_idx, self.minerPos, self.controllerPos, "internal", False,
                                op_blskeys=s_dmn2_keys)

        # Now try with duplicate operator key
        self.log.info("Trying duplicate operator key...")
        assert_raises_rpc_error(-1, "bad-protx-dup-operator-key",
                                self.register_new_dmn, s_dmn2.idx, self.minerPos, self.controllerPos, "internal", False,
                                op_blskeys=s_dmn_keys)

        # Finally, register it properly.
        mns.append(self.register_new_dmn(s_dmn2.idx,
                                         self.minerPos,
                                         self.controllerPos,
                                         "internal",
                                         False))

        self.nodes[s_dmn2.idx].initmasternode(s_dmn2.operator_sk)
        time.sleep(1)
        miner.generate(1)
        self.sync_blocks()
        self.check_mn_enabled_count(2, 2)
        self.check_mn_list(mns)

        # Test payments.
        # Mine 4 blocks and check that each masternode has been paid exactly twice.
        # Save last paid masternode. Check that it's the last paid also after the 4 blocks.
        self.log.info("Testing masternode payments...")
        last_paid_mn = self.get_last_paid_mn()
        starting_balances = {}
        for mn in mns:
            starting_balances[mn.payee] = self.get_addr_balance(controller, mn.payee)
        miner.generate(4)
        self.sync_blocks()
        for mn in mns:
            bal = self.get_addr_balance(controller, mn.payee)
            expected = starting_balances[mn.payee] + Decimal('6.0')
            if bal != expected:
                raise Exception("Invalid balance (%s != %s) for node %d" % (bal, expected, mn.idx))
        self.log.info("All masternodes paid twice.")
        assert_equal(last_paid_mn, self.get_last_paid_mn())
        self.log.info("Order preserved.")


if __name__ == '__main__':
    DIP3ShieldTest().main()
