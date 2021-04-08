cmake_minimum_required(VERSION 3.10)

# Search for and use ccache if available to speed up build times
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    # Set up wrapper scripts
    set(C_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CXX_LAUNCHER "${CCACHE_PROGRAM}")
endif()

project(Sombe)

set(BDB_VER "4.8.30")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/contrib/cmake")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 ")
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    message(FATAL_ERROR "Native Windows CMake is not supported yet, use WSL instead")
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    message(STATUS "Compiling on macOS")
    set(ENV{target} "Mac")
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/qt5)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/openssl@1.1)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/Cellar/berkeley-db@4)
    #set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@1.1/")
    #set(ENV{CPPFLAGS} "-I${OPENSSL_ROOT_DIR}/include")
    #set(ENV{LDFLAGS} "-L${OPENSSL_ROOT_DIR}/lib")
    set(BerkeleyDB_ROOT_DIR "/usr/local/Cellar/berkeley-db@4/${BDB_VER}/")
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_NO_BOOST_CMAKE ON)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(ENV{target} "Linux")
    file(READ "/proc/version" _SYS_VERSION)
    string(REGEX MATCH "Microsoft" _SYSTEM_VERSION_PARSED "${_SYS_VERSION}")
    if(${_SYSTEM_VERSION_PARSED} MATCHES "Microsoft")
        message(STATUS "Compiling on WSL")
        set(ENV{DISPLAY} ":0")
        set(ENV{LIBGL_ALWAYS_INDIRECT} 1)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY /home/$ENV{USER}/Sombe-run)
        message(WARNING "WSL Runtime support requires VcXsrv to be installed and running")
    else()
        message(STATUS "Compiling on Linux")
    endif()
    list(APPEND CMAKE_PREFIX_PATH /usr/lib/x86_64-linux-gnu/cmake/Qt5)
    set(Qt5core_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5/QtCore")
    find_path(ENDIAN_INCLUDES endian.h)
    if(ENDIAN_INCLUDES)
        message(STATUS "Found endian.h: ${ENDIAN_INCLUDES}")
    endif()
    #add_definitions("-D__BYTE_ORDER -D__LITTLE_ENDIAN -D__GLIBC__ -DHAVE_DECL_BSWAP_16=0 -DHAVE_DECL_BSWAP_32=0 -DHAVE_DECL_BSWAP_64=0")
endif()

# Find Dependencies
find_package(BerkeleyDB ${BDB_VER} REQUIRED)
if(BerkeleyDB_FOUND)
    if(NOT ${BerkeleyDB_VERSION} MATCHES "${BDB_VER}")
        message(WARNING "BerkeleyDB version other than ${BDB_VER} found!")
        set(BDB_CONFIGURE_FLAGS "--with-incompatible-bdb")
    endif()
endif()

find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL (${OPENSSL_VERSION}): ${OPENSSL_LIBRARIES}")
    if(OPENSSL_VERSION VERSION_GREATER_EQUAL 1.1)
        message(STATUS "Found unsupported OpenSSL version!")
        set(SSL_CONFIGURE_FLAGS "--with-unsupported-ssl")
    endif()
endif()

find_package(LibEvent REQUIRED)

find_package(GMP)
if(GMP_FOUND)
else()
    message(WARNING "GMP not found, falling back to OpenSSL for bignum!")
    set(BIGNUM_CONFIGURE_FLAGS "--with-zerocoin-bignum=openssl")
endif()

find_package(ZMQ)
find_package(Miniupnp)
find_package(Boost COMPONENTS system filesystem thread program_options REQUIRED)

# run autogen.sh if missing header files from configure on Linux/Mac
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/configure")
else()
    execute_process(
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/autogen.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# run configure if Sombe_config.h doesn't exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/config/Sombe-config.h")
else()
    execute_process(
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/configure ${CONFIGSITE} ${BDB_CONFIGURE_FLAGS} ${BIGNUM_CONFIGURE_FLAGS} ${SSL_CONFIGURE_FLAGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
add_definitions(-DHAVE_CONFIG_H)

ExternalProject_Add (
        libunivalue
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/univalue
        CONFIGURE_COMMAND ""
        BUILD_COMMAND $(MAKE)
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
        )

ExternalProject_Add (
        libsecp256k1
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/secp256k1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND $(MAKE)
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
        )

set(libleveldb_a_headers
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/atomic_pointer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_example.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_posix.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/win/stdint.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_win.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/thread_annotations.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/db.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/options.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/comparator.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/filter_policy.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/slice.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/table_builder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/env.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/c.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/iterator.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/cache.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/dumpfile.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/table.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/write_batch.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/status.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/log_format.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/memtable.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/version_set.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/write_batch_internal.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/filename.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/version_edit.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/dbformat.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/builder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/log_writer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/db_iter.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/skiplist.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/db_impl.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/table_cache.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/snapshot.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/log_reader.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/filter_block.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/block_builder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/block.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/two_level_iterator.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/merger.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/format.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/iterator_wrapper.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/crc32c.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/env_posix_test_helper.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/arena.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/random.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/posix_logger.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/hash.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/histogram.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/coding.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/testutil.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/mutexlock.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/logging.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/testharness.h
        )

set(libleveldb_a_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/builder.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/c.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/dbformat.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/db_impl.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/db_iter.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/dumpfile.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/filename.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/log_reader.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/log_writer.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/memtable.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/repair.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/table_cache.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/version_edit.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/version_set.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/write_batch.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/block_builder.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/block.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/filter_block.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/format.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/iterator.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/merger.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/table_builder.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/table.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/two_level_iterator.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/arena.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/bloom.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/cache.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/coding.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/comparator.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/crc32c.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/env.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/env_posix.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/filter_policy.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/hash.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/histogram.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/logging.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/options.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/status.cc
        )

set(libleveldb_cpp_flags)
if($ENV{target} MATCHES "Windows")
        set(libleveldb_cpp_flags -DOS_WINDOWS -DLEVELDB_PLATFORM_WINDOWS -DWINVER=0x0500 -D__USE_MINGW_ANSI_STDIO=1)
        set(libleveldb_a_sources ${libleveldb_a_sources} ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/env_win.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_win.cc)
else()
    if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(libleveldb_cpp_flags -DOS_MACOSX -DLEVELDB_PLATFORM_POSIX)
    else()
        set(libleveldb_cpp_flags -DOS_LINUX -DLEVELDB_PLATFORM_POSIX)
    endif()
    set(libleveldb_a_sources ${libleveldb_a_sources} ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_posix.cc)
endif()

add_library(leveldb STATIC ${libleveldb_a_headers} ${libleveldb_a_sources})
target_compile_definitions(leveldb PUBLIC ${libleveldb_cpp_flags} -DLEVELDB_ATOMIC_PRESENT -D__STDC_LIMIT_MACROS)
target_include_directories(leveldb PUBLIC ${ENDIAN_INCLUDES}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv
        )

set(libmemenv_a_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv/memenv.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv/memenv.h
        )
add_library(memenv STATIC ${libmemenv_a_sources})
target_compile_definitions(memenv PUBLIC ${libleveldb_cpp_flags})
target_include_directories(memenv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv
        )

set(libleveldb_sse42_a_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_posix_sse.cc)
add_library(leveldb_sse42 ${libleveldb_sse42_a_sources})
target_include_directories(leveldb_sse42 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv
        )

file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
file(GLOB CRYPTO_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/*.h)
file(GLOB PRIMITIVE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/primitives/*.h)
file(GLOB ZMQ_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/zmq/*.h)
file(GLOB SCRIPT_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/script/*.h)
file(GLOB RPC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/*.h)
file(GLOB COMPAT_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/compat/*.h)
file(GLOB CONSENSUS_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/consensus/*.h)
file(GLOB CTAES_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/ctaes/*.h)

source_group("BitcoinHeaders" FILES
        ${HEADERS}
        ${CRYPTO_HEADERS}
        ${PRIMITIVE_HEADERS}
        ${ZMQ_HEADERS}
        ${SCRIPT_HEADERS}
        ${RPC_HEADERS}
        ${COMPAT_HEADERS}
        ${CONSENSUS_HEADERS}
        ${CTAES_HEADERS}
        ./src/support/cleanse.h
        )

set(SERVER_SOURCES
        ./src/addrman.cpp
        ./src/bloom.cpp
        ./src/blocksignature.cpp
        ./src/chain.cpp
        ./src/checkpoints.cpp
        ./src/httprpc.cpp
        ./src/httpserver.cpp
        ./src/init.cpp
        ./src/dbwrapper.cpp
        ./src/main.cpp
        ./src/merkleblock.cpp
        ./src/miner.cpp
        ./src/net.cpp
        ./src/noui.cpp
        ./src/policy/fees.cpp
        ./src/pow.cpp
        ./src/rest.cpp
        ./src/rpc/blockchain.cpp
        ./src/rpc/masternode.cpp
        ./src/rpc/budget.cpp
        ./src/rpc/mining.cpp
        ./src/rpc/misc.cpp
        ./src/rpc/net.cpp
        ./src/rpc/rawtransaction.cpp
        ./src/rpc/server.cpp
        ./src/script/sigcache.cpp
        ./src/sporkdb.cpp
        ./src/timedata.cpp
        ./src/torcontrol.cpp
        ./src/txdb.cpp
        ./src/txmempool.cpp
        ./src/validationinterface.cpp
        )
add_library(SERVER_A STATIC ${BitcoinHeaders} ${SERVER_SOURCES})
if(MINIUPNP_FOUND)
    target_compile_definitions(SERVER_A PUBLIC "-DSTATICLIB -DMINIUPNP_STATICLIB")
endif()
target_include_directories(SERVER_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv
    ${ZMQ_INCLUDE_DIR} ${LIBEVENT_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${BerkeleyDB_INCLUDE_DIRS}
    )

if(ZMQ_FOUND)
    set(ZMQ_SOURCES
        ./src/zmq/zmqabstractnotifier.cpp
        ./src/zmq/zmqnotificationinterface.cpp
        ./src/zmq/zmqpublishnotifier.cpp
    )
    add_library(ZMQ_A STATIC ${BitcoinHeaders} ${ZMQ_SOURCES} ${ZMQ_LIB})
    target_include_directories(ZMQ_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${ZMQ_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
    target_compile_definitions(ZMQ_A PUBLIC "-DZMQ_STATIC")
endif()

set(WALLET_SOURCES
        ./src/activemasternode.cpp
        ./src/bip38.cpp
        ./src/wallet/db.cpp
        ./src/addressbook.cpp
        ./src/crypter.cpp
        ./src/swifttx.cpp
        ./src/masternode.cpp
        ./src/masternode-budget.cpp
        ./src/masternode-payments.cpp
        ./src/masternode-sync.cpp
        ./src/masternodeconfig.cpp
        ./src/masternodeman.cpp
        ./src/messagesigner.cpp
        ./src/wallet/hdchain.cpp
        ./src/wallet/rpcdump.cpp
        ./src/wallet/scriptpubkeyman.cpp
        ./src/wallet/rpcwallet.cpp
        ./src/kernel.cpp
        ./src/legacy/stakemodifier.cpp
        ./src/wallet/wallet.cpp
        ./src/wallet/wallet_ismine.cpp
        ./src/wallet/walletdb.cpp
        ./src/stakeinput.cpp
        )
add_library(WALLET_A STATIC ${BitcoinHeaders} ${WALLET_SOURCES})
target_include_directories(WALLET_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/secp256k1/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
        ${OPENSSL_INCLUDE_DIR}
        ${BerkeleyDB_INCLUDE_DIRS}
        )

set(BITCOIN_CRYPTO_SOURCES
        ./src/crypto/aes.cpp
        ./src/crypto/sha1.cpp
        ./src/crypto/sha256.cpp
        ./src/crypto/sha512.cpp
        ./src/crypto/chacha20.cpp
        ./src/crypto/hmac_sha256.cpp
        ./src/crypto/rfc6979_hmac_sha256.cpp
        ./src/crypto/hmac_sha512.cpp
        ./src/crypto/scrypt.cpp
        ./src/crypto/ripemd160.cpp
        ./src/crypto/aes_helper.c
        ./src/crypto/blake.c
        ./src/crypto/bmw.c
        ./src/crypto/groestl.c
        ./src/crypto/jh.c
        ./src/crypto/keccak.c
        ./src/crypto/skein.c
        ./src/crypto/common.h
        ./src/crypto/sha256.h
        ./src/crypto/sha512.h
        ./src/crypto/chacha20.h
        ./src/crypto/hmac_sha256.h
        ./src/crypto/rfc6979_hmac_sha256.h
        ./src/crypto/hmac_sha512.h
        ./src/crypto/scrypt.h
        ./src/crypto/sha1.h
        ./src/crypto/ripemd160.h
        ./src/crypto/sph_blake.h
        ./src/crypto/sph_bmw.h
        ./src/crypto/sph_groestl.h
        ./src/crypto/sph_jh.h
        ./src/crypto/sph_keccak.h
        ./src/crypto/sph_skein.h
        ./src/crypto/sph_types.h
        )
add_library(BITCOIN_CRYPTO_A STATIC ${BITCOIN_CRYPTO_SOURCES})
target_include_directories(BITCOIN_CRYPTO_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})

set(COMMON_SOURCES
        ./src/allocators.cpp
        ./src/amount.cpp
        ./src/base58.cpp
        ./src/bip38.cpp
        ./src/consensus/params.cpp
        ./src/consensus/upgrades.cpp
        ./src/chainparams.cpp
        ./src/coins.cpp
        ./src/key_io.cpp
        ./src/compressor.cpp
        ./src/consensus/merkle.cpp
        ./src/consensus/tx_verify.cpp
        ./src/primitives/block.cpp
        ./src/primitives/transaction.cpp
        ./src/core_read.cpp
        ./src/core_write.cpp
        ./src/hash.cpp
        ./src/key.cpp
        ./src/keystore.cpp
        ./src/netaddress.cpp
        ./src/netbase.cpp
        ./src/protocol.cpp
        ./src/pubkey.cpp
        ./src/scheduler.cpp
        ./src/script/interpreter.cpp
        ./src/script/script.cpp
        ./src/script/sign.cpp
        ./src/script/standard.cpp
        ./src/script/script_error.cpp
        ./src/spork.cpp
        ./src/sporkdb.cpp
        )
add_library(COMMON_A STATIC ${BitcoinHeaders} ${COMMON_SOURCES})
target_include_directories(COMMON_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/secp256k1/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
        ${OPENSSL_INCLUDE_DIR}
        ${BerkeleyDB_INCLUDE_DIRS}
        )

set(UTIL_SOURCES
        ./src/allocators.cpp
        ./src/compat/strnlen.cpp
        ./src/compat/glibc_sanity.cpp
        ./src/compat/glibcxx_sanity.cpp
        ./src/chainparamsbase.cpp
        ./src/clientversion.cpp
        ./src/fs.cpp
        ./src/logging.cpp
        ./src/random.cpp
        ./src/rpc/protocol.cpp
        ./src/sync.cpp
        ./src/blob_uint256.cpp
        ./src/arith_uint256.cpp
        ./src/uint256.cpp
        ./src/util/threadnames.cpp
        ./src/util.cpp
        ./src/utilstrencodings.cpp
        ./src/utilmoneystr.cpp
        ./src/utiltime.cpp
        ./src/support/cleanse.cpp
        )
add_library(UTIL_A STATIC ${BitcoinHeaders} ${UTIL_SOURCES})
target_include_directories(UTIL_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
        ${OPENSSL_INCLUDE_DIR}
        )

set(CLI_A_SOURCES ./src/rpc/client.cpp)
add_library(CLI_A STATIC ${BitcoinHeaders} ${CLI_A_SOURCES})
target_include_directories(CLI_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
        )

link_directories(
        ${CMAKE_SOURCE_DIR}/src/univalue/.libs
        ${CMAKE_SOURCE_DIR}/src/secp256k1/.libs
        )

set(Sombe-cli_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/Sombe-cli.cpp)
if($ENV{target} MATCHES "Windows")
    list(APPEND Sombe-cli_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/Sombe-cli-res.rc)
endif()
add_executable(Sombe-cli ${Sombe-cli_SOURCES})
add_dependencies(Sombe-cli libunivalue)
target_link_libraries(Sombe-cli
        CLI_A
        univalue
        UTIL_A
        BITCOIN_CRYPTO_A
        ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${LIBEVENT_LIB}
        )
if($ENV{target} MATCHES "Windows")
    target_link_libraries(Sombe-cli ${WINDOWS_LDADD})
endif()

set(Sombe-tx_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/Sombe-tx.cpp)
if($ENV{target} MATCHES "Windows")
    list(APPEND Sombe-tx_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/Sombe-tx-res.rc)
endif()
add_executable(Sombe-tx ${Sombe-tx_SOURCES})
add_dependencies(Sombe-tx libunivalue libsecp256k1)
target_link_libraries(Sombe-tx
        univalue
        COMMON_A
        UTIL_A
        BITCOIN_CRYPTO_A
        secp256k1
        ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${LIBEVENT_LIB}
        )
if($ENV{target} MATCHES "Windows")
    target_link_libraries(Sombe-tx ${WINDOWS_LDADD})
endif()
if(GMP_FOUND)
    target_link_libraries(Sombe-tx ${GMP_LIBRARY})
endif()

link_directories(${CMAKE_BINARY_DIR})

set(Sombed_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/Sombed.cpp)
if($ENV{target} MATCHES "Windows")
    list(APPEND Sombed_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/Sombed-res.rc)
endif()
add_executable(Sombed ${Sombed_SOURCES} ${BitcoinHeaders})
add_dependencies(Sombed libunivalue libsecp256k1 leveldb leveldb_sse42 memenv)
target_include_directories(Sombed PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv
        ${LIBEVENT_INCLUDE_DIR})
target_link_libraries(Sombed
        pthread
        SERVER_A
        WALLET_A
        COMMON_A
        univalue
        UTIL_A
        BITCOIN_CRYPTO_A
        ${CMAKE_BINARY_DIR}/libleveldb.a
        ${CMAKE_BINARY_DIR}/libleveldb_sse42.a
        ${CMAKE_BINARY_DIR}/libmemenv.a
        secp256k1
        ${BerkeleyDB_LIBRARIES} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${LIBEVENT_LIB} pthread
        )
if($ENV{target} MATCHES "Windows")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wstack-protector -fstack-protector-all -fPIE -pipe -O2 -pthread -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -pie --static")
    target_link_libraries(Sombed ${WINDOWS_LDADD})
endif()
if(GMP_FOUND)
    target_link_libraries(Sombed ${GMP_LIBRARY})
    target_include_directories(Sombed PUBLIC ${GMP_INCLUDE_DIR})
endif()
if(ZMQ_FOUND)
    target_link_libraries(Sombed ZMQ_A ${ZMQ_LIB})
    target_include_directories(Sombed PUBLIC ${ZMQ_INCLUDE_DIR})
endif()
if(MINIUPNP_FOUND)
    target_compile_definitions(Sombed PUBLIC "-DSTATICLIB -DMINIUPNP_STATICLIB")
    target_link_libraries(Sombed ${MINIUPNP_LIBRARY})
    target_include_directories(Sombed PUBLIC ${MINIUPNP_INCLUDE_DIR})
endif()

target_link_libraries(Sombed -lpthread)

add_subdirectory(src/qt)
