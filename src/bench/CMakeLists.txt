CMAKE_MINIMUM_REQUIRED(VERSION 3.14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS system filesystem chrono thread unit_test_framework REQUIRED)

set(RAW_TEST_FILES  ${CMAKE_CURRENT_SOURCE_DIR}/data/block2680960.raw)

# Generate raw files
function(GenerateRaws)
    set(fileList "")
    foreach(file IN LISTS ARGN)
        get_filename_component(filename ${file} NAME_WE)
        set(outFile ${file}.h)
        set(runCmd ${CMAKE_SOURCE_DIR}/contrib/devtools/hexdump_util.sh)
        add_custom_command(
                OUTPUT ${outFile}
                COMMAND ${CMAKE_COMMAND} -E echo "static unsigned const char ${filename}_raw[] = {" > ${outFile}
                COMMAND ${runCmd} ${file} ${outFile}
                COMMAND ${CMAKE_COMMAND} -E echo "};" >> ${outFile}
                DEPENDS ${file}
                COMMENT "Generating raw ${file}.h"
                VERBATIM
        )
        list(APPEND fileList ${outFile})
    endforeach()
    install(FILES ${fileList} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/data)
    add_custom_target(
            genRaws2 ALL
            DEPENDS ${fileList}
            COMMENT "Processing raw files..."
    )
endfunction()

GenerateRaws(${RAW_TEST_FILES})

set(BITCOIN_BENCH_SUITE
        ${CMAKE_CURRENT_SOURCE_DIR}/bench_hemis.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bench.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bench.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Examples.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/base58.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bls.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bls_dkg.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/checkblock.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/checkqueue.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/data.h
        ${CMAKE_CURRENT_SOURCE_DIR}/data.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/chacha20.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto_hash.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ecdsa.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/lockedpool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/perf.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/perf.h
        ${CMAKE_CURRENT_SOURCE_DIR}/prevector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rollingbloom.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/util_time.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/walletprocessblock.cpp
        )

set(bench_bench_hemis_SOURCES ${BITCOIN_BENCH_SUITE} ${BITCOIN_TESTS})
add_executable(bench_hemis ${bench_bench_hemis_SOURCES} ${BitcoinHeaders})
add_dependencies(bench_hemis genHeaders genRaws2 libunivalue libsecp256k1 libzcashrust leveldb crc32c bls)
target_include_directories(bench_hemis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/leveldb
        ${CMAKE_SOURCE_DIR}/src/leveldb/include
        ${CMAKE_SOURCE_DIR}/src/leveldb/helpers/memenv
        ${LIBEVENT_INCLUDE_DIR}
        ${GMP_INCLUDE_DIR})
target_link_libraries(bench_hemis PRIVATE
        SERVER_A
        CLI_A
        WALLET_A
        COMMON_A
        univalue
        ZEROCOIN_A
        UTIL_A
        SAPLING_A
        BITCOIN_CRYPTO_A
        leveldb
        crc32c
        secp256k1
        rustzcash
        bls
        ${BerkeleyDB_LIBRARIES} ${Boost_LIBRARIES} ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} ${LIBEVENT_LIB} ${GMP_LIBRARY} pthread
        )
if(ZMQ_FOUND)
    target_link_libraries(bench_hemis PRIVATE ZMQ_A ${ZMQ_LIB})
    target_include_directories(bench_hemis PRIVATE ${ZMQ_INCLUDE_DIR})
endif()
if(MINIUPNP_FOUND)
    target_compile_definitions(bench_hemis PRIVATE "-DSTATICLIB -DMINIUPNP_STATICLIB")
    target_link_libraries(bench_hemis PRIVATE ${MINIUPNP_LIBRARY})
    target_include_directories(bench_hemis PRIVATE ${MINIUPNP_INCLUDE_DIR})
endif()
if(NAT-PMP_FOUND)
    target_link_libraries(bench_hemis PRIVATE ${NAT-PMP_LIBRARY})
    target_include_directories(bench_hemis PRIVATE ${NAT-PMP_INCLUDE_DIR})
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(bench_hemis PRIVATE "-framework Cocoa")
endif()

target_link_libraries(bench_hemis PRIVATE ${sodium_LIBRARY_RELEASE} -ldl -lpthread)

