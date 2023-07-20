#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin Core developers
# Copyright (c) 2022 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the scantxoutset rpc call."""
from test_framework.test_framework import PivxTestFramework
from test_framework.util import assert_equal

from decimal import Decimal
import shutil
import os

class ScantxoutsetTest(PivxTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.log.info("Mining blocks...")
        self.nodes[0].generate(110)

        addr1 = self.nodes[0].getnewaddress("")
        pubk1 = self.nodes[0].validateaddress(addr1)['pubkey']
        addr2 = self.nodes[0].getnewaddress("")
        pubk2 = self.nodes[0].validateaddress(addr2)['pubkey']
        addr3 = self.nodes[0].getnewaddress("")
        pubk3 = self.nodes[0].validateaddress(addr3)['pubkey']
        self.nodes[0].sendtoaddress(addr1, 0.001)
        self.nodes[0].sendtoaddress(addr2, 0.002)
        self.nodes[0].sendtoaddress(addr3, 0.004)

        #send to child keys of DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V
        self.nodes[0].sendtoaddress("y1kNaESNDkcshcLsFJ53XYsJi21N1qjTwv", 0.008) # (m/0'/0'/0')
        self.nodes[0].sendtoaddress("xzHN1U7N3mzrksMMZ8jpMtm1Qc2BzMZhQc", 0.016) # (m/0'/0'/1')
        self.nodes[0].sendtoaddress("yJaWjJzAEsypAV4JkLRmpjrtqQ3X8NfXiW", 0.032) # (m/0'/0'/1500')
        self.nodes[0].sendtoaddress("y6u2zs2CfRJhu69fed1AuiR1uK6DGM3Jzo", 0.064) # (m/0'/0'/0)
        self.nodes[0].sendtoaddress("y3vZeiqaj6q8bRUkR4F2ojrdjDnF5vcA2x", 0.128) # (m/0'/0'/1)
        self.nodes[0].sendtoaddress("y27mmfSF2X81Ymv8td9jnovCuo985wduP4", 0.256) # (m/0'/0'/1500)
        self.nodes[0].sendtoaddress("xzbsq3Cf4fc7QfiMt9Liy4gsLvPuvSTqNf", 0.512) # (m/1/1/0')
        self.nodes[0].sendtoaddress("xwFDPMkJYwmFUw8SQHEGwA1S6AbtkCz2Vn", 1.024) # (m/1/1/1')
        self.nodes[0].sendtoaddress("y5MzBDMpTRzKGSFSZcJRg43yQkHHaNqpvZ", 2.048) # (m/1/1/1500')
        self.nodes[0].sendtoaddress("yA8NNWw22P3Rtf4rWgLj2naYbw26nu8X4k", 4.096) # (m/1/1/0)
        self.nodes[0].sendtoaddress("yEH1WA5CK49MzU2saBMKR1YvWBVguZmATi", 8.192) # (m/1/1/1)
        self.nodes[0].sendtoaddress("y5s2Rr6Earyz52Cq6qa2366oMHjA4ytdk3", 16.384) # (m/1/1/1500)


        self.nodes[0].generate(1)

        self.log.info("Stop node, remove wallet, mine again some blocks...")
        self.stop_node(0)
        shutil.rmtree(os.path.join(self.nodes[0].datadir, "regtest", 'wallets'))
        self.start_node(0)
        self.nodes[0].generate(110)

        self.restart_node(0, ['-nowallet'])
        self.log.info("Test if we have found the non HD unspent outputs.")
        assert_equal(self.nodes[0].scantxoutset("start", [ "pkh(" + pubk1 + ")", "pkh(" + pubk2 + ")", "pkh(" + pubk3 + ")"])['total_amount'], Decimal("0.007"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(" + pubk1 + ")", "combo(" + pubk2 + ")", "combo(" + pubk3 + ")"])['total_amount'], Decimal("0.007"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "addr(" + addr1 + ")", "addr(" + addr2 + ")", "addr(" + addr3 + ")"])['total_amount'], Decimal("0.007"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "addr(" + addr1 + ")", "addr(" + addr2 + ")", "combo(" + pubk3 + ")"])['total_amount'], Decimal("0.007"))

        self.log.info("Test extended key derivation.")
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/0'/0h/0h)"])['total_amount'], Decimal("0.008"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/0'/0'/1h)"])['total_amount'], Decimal("0.016"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/0h/0'/1500')"])['total_amount'], Decimal("0.032"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/0h/0h/0)"])['total_amount'], Decimal("0.064"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/0'/0h/1)"])['total_amount'], Decimal("0.128"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/0h/0'/1500)"])['total_amount'], Decimal("0.256"))
        assert_equal(self.nodes[0].scantxoutset("start", [ {"desc": "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/0'/0h/*h)", "range": 1499}])['total_amount'], Decimal("0.024"))
        assert_equal(self.nodes[0].scantxoutset("start", [ {"desc": "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/0'/0'/*h)", "range": 1500}])['total_amount'], Decimal("0.056"))
        assert_equal(self.nodes[0].scantxoutset("start", [ {"desc": "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/0h/0'/*)", "range": 1499}])['total_amount'], Decimal("0.192"))
        assert_equal(self.nodes[0].scantxoutset("start", [ {"desc": "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/0'/0h/*)", "range": 1500}])['total_amount'], Decimal("0.448"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/1/1/0')"])['total_amount'], Decimal("0.512"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/1/1/1')"])['total_amount'], Decimal("1.024"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/1/1/1500h)"])['total_amount'], Decimal("2.048"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/1/1/0)"])['total_amount'], Decimal("4.096"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/1/1/1)"])['total_amount'], Decimal("8.192"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/1/1/1500)"])['total_amount'], Decimal("16.384"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKVrRjogj3bNiLD8T7yviZvdFiEmRZcQktS3TuQ9au4Md7YzJgc7RZ4P4fkj9s5WMk5scoiNRQM2pUq7x9XGiwH7YDSFiJJJaGrCCro2yXUob2k/1/1/0)"])['total_amount'], Decimal("4.096"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKVrRjogj3bNiLD8T7yviZvdFiEmRZcQktS3TuQ9au4Md7YzJgc7RZ4P4fkj9s5WMk5scoiNRQM2pUq7x9XGiwH7YDSFiJJJaGrCCro2yXUob2k/1/1/1)"])['total_amount'], Decimal("8.192"))
        assert_equal(self.nodes[0].scantxoutset("start", [ "combo(DRKVrRjogj3bNiLD8T7yviZvdFiEmRZcQktS3TuQ9au4Md7YzJgc7RZ4P4fkj9s5WMk5scoiNRQM2pUq7x9XGiwH7YDSFiJJJaGrCCro2yXUob2k/1/1/1500)"])['total_amount'], Decimal("16.384"))
        assert_equal(self.nodes[0].scantxoutset("start", [ {"desc": "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/1/1/*')", "range": 1499}])['total_amount'], Decimal("1.536"))
        assert_equal(self.nodes[0].scantxoutset("start", [ {"desc": "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/1/1/*')", "range": 1500}])['total_amount'], Decimal("3.584"))
        assert_equal(self.nodes[0].scantxoutset("start", [ {"desc": "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/1/1/*)", "range": 1499}])['total_amount'], Decimal("12.288"))
        assert_equal(self.nodes[0].scantxoutset("start", [ {"desc": "combo(DRKPuUWy7gEYo13wzMa8dntopw4ZJ7rKddnhnhi3f9tGkaDNpKSSPqMVwxqgTnZhrExKAUSZSo8uKnzcEkjFae1udSMQcvecJbLdmi9PHCETZy7V/1/1/*)", "range": 1500}])['total_amount'], Decimal("28.672"))
        assert_equal(self.nodes[0].scantxoutset("start", [ {"desc": "combo(DRKVrRjogj3bNiLD8T7yviZvdFiEmRZcQktS3TuQ9au4Md7YzJgc7RZ4P4fkj9s5WMk5scoiNRQM2pUq7x9XGiwH7YDSFiJJJaGrCCro2yXUob2k/1/1/*)", "range": 1499}])['total_amount'], Decimal("12.288"))
        assert_equal(self.nodes[0].scantxoutset("start", [ {"desc": "combo(DRKVrRjogj3bNiLD8T7yviZvdFiEmRZcQktS3TuQ9au4Md7YzJgc7RZ4P4fkj9s5WMk5scoiNRQM2pUq7x9XGiwH7YDSFiJJJaGrCCro2yXUob2k/1/1/*)", "range": 1500}])['total_amount'], Decimal("28.672"))

if __name__ == '__main__':
    ScantxoutsetTest().main()
