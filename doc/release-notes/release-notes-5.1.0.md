hemis Core version v5.1.0 is now available from:  <https://github.com/hemis-project/hemis/releases>

This is a new major version release, including various bug fixes and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github: <https://github.com/hemis-project/hemis/issues>

Recommended Update
==============

This version is an optional, but recommended, update for all users and services.

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/hemis-Qt (on Mac) or hemisd/hemis-qt (on Linux).

Sapling Parameters
==================

In order to run, hemis Core now requires two files, `sapling-output.params` and `sapling-spend.params` (with total size ~50 MB), to be saved in a specific location.

For the following packages, no action is required by the user:
- macOS release `dmg` binaries will use the params that are bundled into the .app bundle.
- Windows installer `.exe` will automatically copy the files in the proper location.
- Linux `PPA/Snap` installs will automatically copy the files in the proper location.

For the other packages, the user must save the param files in the proper location, before being able to run hemis v5.0.0:
- macOS/Linux `tar.gz` tarballs include a bash script (`install-params.sh`) to copy the parameters in the appropriate location.
- Windows `.zip` users need to manually copy the files from the `share/hemis` folder to the `%APPDATA%\hemisParams` directory.
- self compilers can run the script from the repository sources (`params/install-params.sh`), or copy the files directly from the `params` subdirectory.

Compatibility
==============

hemis Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.10+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

Apple released it's last Mountain Lion update August 13, 2015, and officially ended support on [December 14, 2015](http://news.fnal.gov/2015/10/mac-os-x-mountain-lion-10-8-end-of-life-december-14/). hemis Core software starting with v3.2.0 will no longer run on MacOS versions prior to Yosemite (10.10). Please do not report issues about MacOS versions prior to Yosemite to the issue tracker.

hemis Core should also work on most other Unix-like systems but is not frequently tested on them.

Notable Changes
==============

- Faster blockchain synchronization and reindex.
- Faster GUI and overall responsiveness.
- Improved chain-state corruption recovery.
- Improved core performance and stability.
- A good amount of bugs fixed.

#### Disable PoW mining RPC Commands

A new configure flag has been introduced to allow more granular control over weather or not the PoW mining RPC commands are compiled into the wallet. By default they are not. This behavior can be overridden by passing `--enable-mining-rpc` to the `configure` script.

v5.1.0 Change log
==============

Detailed release notes follow. This overview includes changes that affect behavior, not code moves, refactors and string updates. For convenience in locating the code changes and accompanying discussion, both the pull request and git merge commit are mentioned.

### Block and transaction handling
- #2150 Revamping block validation interface interaction with wallet (furszy)
- #2179 [Backport] Make CTransaction actually immutable (furszy)
- #2192 Support for validation interface signals in background thread (furszy)
- #2203 Call wallet/validation notify callbacks in scheduler thread (without cs_main) (furszy)
- #2213 Remove dead zc checks from ATMP (furszy)
- #2226 [Refactoring] Prune invalid UTXOs from the coins cache (random-zebra)
- #2295 [Net_Processing] Cleaner block processing workflow (furszy)
- #2290 [Test] Fix concurrency issues in ActivateBestChain + optimization (furszy)
- #2284 Removing unneeded block serialization and signal from ABC (furszy)
- #2243 [Protocol] Block version 9 (random-zebra)

### P2P protocol and network code
- #2090 Fail instead of truncate command name in CMessageHeader (random-zebra)
- #2118 Inv, mempool and validation improvements (furszy)
- #2172 Improve and document SOCKS code (random-zebra)

### Wallet
- #2126 [Backport] Wallet database handling abstractions/simplifications (furszy)
- #2044 Making CWalletTx store TransactionRef (furszy)
- #2117 [BUG][Wallet] Restore AddressBook when marking used keys in the keypool (random-zebra)
- #2197 Close DB on error (Fuzzbawls)
- #2195 Solve an incorrect MakeTransactionRef usage for ATMP call inside the wallet and unit tests (furszy)
- #2201 CTransactionRef migration + CWalletTx/CMerkleTx refactor (furszy)
- #2218 Guarding wallet access from init and print error for unknown MN collaterals (furszy)
- #2232 Berkeley DB v6 compatibility fix (random-zebra)
- #2225 Rescan chain for wallet transactions process re-written (furszy)
- #2209 Killing wallet and GUI cs_main locks (furszy)

### RPC and other APIs
- #2141 Fix argument parsing and example for getbalance (Fuzzbawls)
- #2147 fix getbalance transparent watch-only amount (furszy)
- #2154 `validateaddress` remove duplicate return address value (furszy)
- #2157 mnbudgetvote generalized, duplicated code cleaned (furszy)
- #2177 [BUG] Fix getblockindexstats-getfeeinfo with shield txes (random-zebra)
- #2283 `getwalletinfo`: Add last_processed_block return value (furszy)

### GUI
- #2173 Prevent invalid dashboard txs list filter value loaded from settings (furszy)
- #2178 [BUG][GUI] Fix CWalletTx* casts to CTransaction* (random-zebra)
- #2161 send widget, hide contacts menu when clear event is triggered (furszy)
- #2174 Use wchar_t API explicitly on Windows (Fuzzbawls)
- #2219 FAQ remove freshdesk and SwiftX mention (furszy)
- #2211 Reduce whitespace around qr code fixes #1562 (dnchk)
- #2228 Fixing few shutdown problems (furszy)
- #2229 Transaction filter, minimize model data request + cleanup (furszy)
- #2244 Pre-5.1 make translate (Fuzzbawls)
- #2249 Make box of hemis Send address return to purple when it's empty (PicklesRcool)
- #2247 Fix double fade-in Topbar Available click animation. Issue 2216 (PicklesRcool)
- #2237 Set fee to highest possible if input is too big (dnchk)
- #2215 Automatically set the lowest possible Custom Fee when user provided fee is too low (MishaPozdnikin)
- #2321 [GUI] Fix Cold Staking address list (Fuzzbawls)
- #2327 [BUG] Fix crash when sending shield notes from the GUI with coin control (random-zebra)

### Tests and QA
- #2144 [BUG] Fix random range in feature_blockhashcache (random-zebra)
- #2146 Add checkblock benchmark test (random-zebra)
- #2148 [Core] Prevector Optimizations 2 (random-zebra)
- #2165 Check for diversified address equal to default one in zkeys_test (random-zebra)
- #2132 Add more cases to sighash_tests for malleated txes (random-zebra)
- #2153 Don't create dust outputs in small_txpuzzle_randfee (random-zebra)
- #2169 Introduce TestChainSetup fixture, pre-creating a N-blocks chain (random-zebra)
- #2190 Fix and enable zmq functional test in the test suite (furszy)
- #2176 Double spend inputs and notes unit tests coverage (furszy)
- #2194 [Backport] Disable the mempool P2P command when bloom filters disabled (furszy)
- #2189 [Bug][Refactoring] Miscellaneous fixes (random-zebra)
- #2206 script standard tests back ported (furszy)
- #2214 Fix various things -fsanitize complains about (random-zebra)
- #2221 updating rpc_invalidateblock.py to latest upstream (furszy)
- #2233 fix miner_tests block generation race condition (furszy)
- #2235 address.py: fix invalid `keyhash_to_p2pkh()` (furszy)
- #2254 sync_blocks and sync_mempool improvements (random-zebra)

### Build system
- #2152 [CMake] Fix CMake builds on macOS Big Sur (Fuzzbawls)
- #2105 Don't compile PoW mining RPC commands by default (Fuzzbawls)
- #2198 [GA] Enable CMake macOS Build (Fuzzbawls)
- #2227 fixing gitian build fail with checksum mismatch (furszy)
- #2230 [GA] Dont run the feature_dbcrash test in GA (Fuzzbawls)

### Tier Two Network (Masternodes / Budget)
- #2143 [BUG] Fix deadlock in GetNextMasternodeInQueueForPayment (random-zebra)
- #2170 Better hemis.conf Masternode data error notification (furszy)
- #2186 Improving masternode check inputs and add process (furszy)

### Miscellaneous
- #2082 Generic event handler interface (furszy)
- #2155 [Miner] Rewrite miner code with proper encapsulation + test coverage (furszy)
- #2162 [Refactoring] Avoid dereference-of-casted-pointer (random-zebra)
- #2166 [BUG] Flush the chainstate after pruning invalid entries (random-zebra)
- #2167 [Logs] Add new "sporks" debug logging category (Fuzzbawls)
- #2185 [Backport] Use static_cast instead of C-style casts for non-fundamental types (furszy)
- #2188 [Core] More efficient rolling Bloom filter (random-zebra)
- #2191 [BugFix] Shutdown, stop threadGroup before dump data to disk (furszy)
- #2207 Use non-atomic flushing with block replay (furszy)
- #2205 logging: Remove unused return value from logprintstr (random-zebra)
- #2223 [BUG] Fix some chainstate-init-order bugs (random-zebra)
- #2224 Removing `TX_ZEROCOINMINT` and `ScriptPubKeyToJSON` (furszy)

### Documentation
- #2187 WSL Depends builds need --disable-online-rust (Fuzzbawls)
- #2231 More Descriptive Budget RPCs (Liquid369)
- #2262 add reduce-memory.md (furszy)

### Refactoring
- #2089 Fix some -Wdeprecated-copy warnings (random-zebra)
- #2092 [Core] Migrate uint160 (CKeyID/CScriptID) to opaque blobs (random-zebra)
- #1974 [Refactoring] Use c++11 list initialization for maps/vectors (Fuzzbawls)
- #2168 [Tests] Fix tests order on test_runner --all (random-zebra)
- #2135 [Masternode] Manage status cleanup (furszy)
- #2184 Grouping misbehaving / cs_main lock calls into a single place (furszy)
- #2175 [Refactoring] Remove g_IsSapling active flag (random-zebra)
- #2164 Several updates to the base chain params structure (furszy)
- #2200 Minor content change for more precise description (NoobieDev12)
- #2181 [Refactoring] Sapling classes - initialize fields and pass parameters by reference (random-zebra)
- #2182 Default initialization for scalar and pointer fields (random-zebra)
- #2180 [Refactoring] Remove BaseOutPoint::GetHash (random-zebra)
- #2204 [Refactoring] Verify MessageSigner signatures with keyIDs (random-zebra)
- #2210 Implement CheckColdStake in TransactionSignatureChecker (random-zebra)
- #2212 [Misc] Init fixes + DisconnectBlock cleanup + zerocoin dead code removal (furszy)

## Credits

Thanks to everyone who directly contributed to this release:

- Akio Nakamura
- Alex Morcos
- Alexey Vesnin
- Ambassador
- Antoine Riard
- Ben Woosley
- Bernhard M. Wiedemann
- Casey Rodarmor
- Chun Kuan Lee
- Cory Fields
- Dan Raviv
- dnchk
- Evan Klitzke
- fanquake
- furszy
- Fuzzbawls
- Hennadii Stepanov
- instagibbs
- Jeremy Rubin
- Jesse Cohen
- John Newbery
- Jonas Schnelli
- Jorge Timón
- João Barbosa
- Karl-Johan Alm
- Lenny Maiorani
- Liquid369
- Luke Dashjr
- Marco
- MarcoFalke
- Matt Corallo
- MishaPozdnikin
- Peter Todd
- PeterL73
- pierrenn
- Pieter Wuille
- practicalswift
- random-zebra
- Suhas Daftuar
- Volodia
- Willy Ko
- Wladimir J. van der Laan


As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/hemis-project-translations/), the QA team during Testing and the Node hosts supporting our Testnet.