#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the walletupgrade functionality.

- 1) start one hemisd node from an pre-HD wallet wallet.dat file localized in util/data
- 2) dumpwallet and get all the keys.
- 3) Upgrade Wallet: stopnode and initialize it with the -upgradewallet flag.
- 4) Verify that all of the pre-HD keys are still in the upgraded wallet.
- 5) Dry out the pre-HD keypool.
- 6) Generate new addresses and verify HD path correctness.
"""

import os
import shutil

from test_framework.test_framework import HemisTestFramework
from test_framework.util import assert_equal


def read_dump(file_name, addrs):
    """
    Read the given dump, count the addrs that match, count change and reserve.
    Also check that the old hd_master is inactive
    """
    with open(file_name, encoding='utf8') as inputfile:
        found_addr = 0
        found_addr_rsv = 0
        for line in inputfile:
            # only read non comment lines
            if line[0] != "#" and len(line) > 10:
                # split out some data
                key_date_label, comment = line.split("#")
                key_date_label = key_date_label.split(" ")

                date = key_date_label[1]
                keytype = key_date_label[2]

                imported_key = date == '1970-01-01T00:00:01Z'
                if imported_key:
                    # Imported keys have multiple addresses, no label (keypath) and timestamp
                    # Skip them
                    continue

                addr_keypath = comment.split(" addr=")[1]
                addr = addr_keypath.split(" ")[0]

                if keytype == "reserve=1":
                    found_addr_rsv += 1
                else:
                    found_addr += 1

                # Ret addresses
                addrs.append(addr)

        return found_addr, found_addr_rsv

def copyPreHDWallet(tmpdir, createFolder):
    destinationDirPath = os.path.join(tmpdir, "node0", "regtest")
    if createFolder:
        os.makedirs(destinationDirPath)
    destPath = os.path.join(destinationDirPath, "wallet.dat")
    sourcePath = os.path.join("test", "util", "data", "pre_hd_wallet.dat")
    shutil.copyfile(sourcePath, destPath)

class WalletUpgradeTest (HemisTestFramework):

    def setup_chain(self):
        self._initialize_chain_clean()
        copyPreHDWallet(self.options.tmpdir, True)

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def check_keys(self, addrs):
        self.log.info("Checking old keys existence in the upgraded wallet..")
        # Now check that all of the pre upgrade addresses are still in the wallet
        for addr in addrs:
            vaddr = self.nodes[0].getaddressinfo(addr)
            assert_equal(vaddr['ismine'], True)
            assert_equal('hdseedid' in vaddr, False)

        self.log.info("All pre-upgrade keys found in the wallet :)")

        # Use all of the keys in the pre-HD keypool
        for _ in range(0, 60):
            self.nodes[0].getnewaddress()

        self.log.info("All pre-upgrade keys should have been marked as used by now, creating new HD keys")

        # Try generating 10 new addresses and verify that those are HD now
        for i in range(0, 10):
            addrHD = self.nodes[0].getnewaddress()
            vaddrHD = self.nodes[0].getaddressinfo(addrHD)  # required to get hd keypath
            assert_equal('hdseedid' in vaddrHD, True)
            assert_equal(vaddrHD['hdkeypath'], "m/44'/119'/0'/0'/" +str(i)+"'")


    def run_test(self):
        # Make sure we use hd
        if '-legacywallet' in self.nodes[0].extra_args:
            self.log.info("Exiting HD upgrade test for non-HD wallets")
            return
        self.log.info("Checking correct version")
        assert_equal(self.nodes[0].getwalletinfo()['walletversion'], 61000)

        self.log.info("Dumping pre-HD wallet")
        prevHDWalletDumpFile = os.path.join(self.options.tmpdir, "node0", "wallet.dump")
        self.nodes[0].dumpwallet(prevHDWalletDumpFile)

        test_addr_rsv_count = 60 # Prev HD wallet reserve keypool (the wallet was initialized with -keypool=60)
        test_addr_count = 21 # Amount of generated addresses (20 manually + 1 created by default)
        addrs = []
        found_addr, found_addr_rsv = \
            read_dump(prevHDWalletDumpFile, addrs)
        assert_equal(found_addr, test_addr_count)  # all keys must be in the dump
        assert_equal(found_addr_rsv, test_addr_rsv_count)

        self.log.info("Upgrading wallet to HD..")
        # Now that know that the wallet is ok, let's upgrade it.
        self.stop_node(0)
        self.start_node(0, ["-upgradewallet"])

        # Now check if the upgrade went fine
        self.check_keys(addrs)
        self.log.info("New HD addresses created successfully")

        # Now test the upgrade at runtime using the JSON-RPC upgradewallet command
        self.log.info("## Testing the upgrade via RPC now, stopping the node...")
        self.stop_node(0)
        copyPreHDWallet(self.options.tmpdir, False)
        self.start_node(0)

        # Generating a block to not be in IBD, here we create a new key for the coinbase script
        self.nodes[0].generate(1)

        self.log.info("Upgrading wallet..")
        self.nodes[0].upgradewallet()

        self.log.info("upgrade completed, checking keys now..")
        # Now check if the upgrade went fine
        self.check_keys(addrs)

        self.log.info("Upgrade via RPC completed, all good :)")

if __name__ == '__main__':
    WalletUpgradeTest().main()
