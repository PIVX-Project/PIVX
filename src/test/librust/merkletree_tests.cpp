// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_hemis.h"

#include "test/data/merkle_roots_sapling.json.h"
#include "test/data/merkle_serialization_sapling.json.h"
#include "test/data/merkle_witness_serialization_sapling.json.h"
#include "test/data/merkle_path_sapling.json.h"
#include "test/data/merkle_commitments_sapling.json.h"

#include <iostream>
#include <stdexcept>

#include "utilstrencodings.h"
#include "version.h"
#include "serialize.h"
#include "streams.h"

#include "sapling/incrementalmerkletree.h"
#include "sapling/sapling_util.h"

#include "json_test_vectors.h"

#include <boost/test/unit_test.hpp>


BOOST_FIXTURE_TEST_SUITE(sapling_merkletree_tests, BasicTestingSetup)

template<typename A, typename B, typename C>
void expect_ser_test_vector(B& b, const C& c, const A& tree) {
    expect_test_vector<B, C>(b, c);
}

template<typename Tree, typename Witness>
void test_tree(
        UniValue commitment_tests,
        UniValue root_tests,
        UniValue ser_tests,
        UniValue witness_ser_tests,
        UniValue path_tests
)
{
    size_t witness_ser_i = 0;
    size_t path_i = 0;

    Tree tree;

    // The root of the tree at this point is expected to be the root of the
    // empty tree.
    BOOST_CHECK(tree.root() == Tree::empty_root());

    // The tree doesn't have a 'last' element added since it's blank.
    BOOST_CHECK_THROW(tree.last(), std::runtime_error);

    // The tree is empty.
    BOOST_CHECK(tree.size() == 0);

    // We need to witness at every single point in the tree, so
    // that the consistency of the tree and the merkle paths can
    // be checked.
    std::vector<Witness> witnesses;

    for (size_t i = 0; i < 16; i++) {
        uint256 test_commitment = uint256S(commitment_tests[i].get_str());

        // Witness here
        witnesses.push_back(tree.witness());

        // Now append a commitment to the tree
        tree.append(test_commitment);

        // Size incremented by one.
        BOOST_CHECK(tree.size() == i+1);

        // Last element added to the tree was `test_commitment`
        BOOST_CHECK(tree.last() == test_commitment);

        // Check tree root consistency
        expect_test_vector(root_tests[i], tree.root());

        // Check serialization of tree
        expect_ser_test_vector(ser_tests[i], tree, tree);

        bool first = true; // The first witness can never form a path
        for (Witness& wit : witnesses) {
            // Append the same commitment to all the witnesses
            wit.append(test_commitment);

            if (first) {
                BOOST_CHECK_THROW(wit.path(), std::runtime_error);
                BOOST_CHECK_THROW(wit.element(), std::runtime_error);
            } else {
                auto path = wit.path();
                expect_test_vector(path_tests[path_i++], path);
            }

            // Check witness serialization
            expect_ser_test_vector(witness_ser_tests[witness_ser_i++], wit, tree);

            BOOST_CHECK(wit.root() == tree.root());

            first = false;
        }
    }

    {
        // Tree should be full now
        BOOST_CHECK_THROW(tree.append(uint256()), std::runtime_error);

        for (Witness& wit : witnesses) {
            BOOST_CHECK_THROW(wit.append(uint256()), std::runtime_error);
        }
    }
}

#define MAKE_STRING(x) std::string((x), (x)+sizeof(x))

BOOST_AUTO_TEST_CASE(SaplingVectors) {
    UniValue root_tests = read_json(MAKE_STRING(json_tests::merkle_roots_sapling));
    UniValue ser_tests = read_json(MAKE_STRING(json_tests::merkle_serialization_sapling));
    UniValue witness_ser_tests = read_json(MAKE_STRING(json_tests::merkle_witness_serialization_sapling));
    UniValue path_tests = read_json(MAKE_STRING(json_tests::merkle_path_sapling));
    UniValue commitment_tests = read_json(MAKE_STRING(json_tests::merkle_commitments_sapling));

    test_tree<SaplingTestingMerkleTree, SaplingTestingWitness>(
            commitment_tests,
            root_tests,
            ser_tests,
            witness_ser_tests,
            path_tests
    );
}

BOOST_AUTO_TEST_CASE(emptyroots) {
    libzcash::EmptyMerkleRoots<64, libzcash::SHA256Compress> emptyroots;
    std::array<libzcash::SHA256Compress, 65> computed;

    computed.at(0) = libzcash::SHA256Compress::uncommitted();
    BOOST_CHECK(emptyroots.empty_root(0) == computed.at(0));
    for (size_t d = 1; d <= 64; d++) {
    computed.at(d) = libzcash::SHA256Compress::combine(computed.at(d-1), computed.at(d-1), d-1);
    BOOST_CHECK(emptyroots.empty_root(d) == computed.at(d));
    }

    // Double check that we're testing (at least) all the empty roots we'll use.
    BOOST_CHECK(INCREMENTAL_MERKLE_TREE_DEPTH <= 64);
}

BOOST_AUTO_TEST_CASE(EmptyrootsSapling) {
    libzcash::EmptyMerkleRoots<62, libzcash::PedersenHash> emptyroots;
    std::array<libzcash::PedersenHash, 63> computed;

    computed.at(0) = libzcash::PedersenHash::uncommitted();
    BOOST_CHECK(emptyroots.empty_root(0) == computed.at(0));
    for (size_t d = 1; d <= 62; d++) {
        computed.at(d) = libzcash::PedersenHash::combine(computed.at(d-1), computed.at(d-1), d-1);
        BOOST_CHECK(emptyroots.empty_root(d) == computed.at(d));
    }

    // Double check that we're testing (at least) all the empty roots we'll use.
    BOOST_CHECK(INCREMENTAL_MERKLE_TREE_DEPTH <= 62);
}

BOOST_AUTO_TEST_CASE(EmptyrootSapling) {
    // This literal is the depth-32 empty tree root with the bytes reversed to
    // account for the fact that uint256S() loads a big-endian representation of
    // an integer which converted to little-endian internally.
    uint256 expected = uint256S("3e49b5f954aa9d3545bc6c37744661eea48d7c34e3000d82b7f0010c30f4c2fb");

    BOOST_CHECK(SaplingMerkleTree::empty_root() == expected);
}

BOOST_AUTO_TEST_SUITE_END()
