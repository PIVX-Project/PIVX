/// \file       ParamGeneration.h
///
/// \brief      Parameter generation routines for Zerocoin.
///
/// \author     Ian Miers, Christina Garman and Matthew Green
/// \date       June 2013
///
/// \copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
/// \license    This project is released under the MIT license.
// Copyright (c) 2017-2021 The hemis Core developers

#ifndef PARAMGENERATION_H_
#define PARAMGENERATION_H_

#include "Params.h"

namespace libzerocoin {

void CalculateParams(ZerocoinParams &params, const CBigNum& N, const std::string& aux, uint32_t securityLevel);
void calculateGroupParamLengths(uint32_t maxPLen, uint32_t securityLevel, uint32_t *pLen, uint32_t *qLen);

// Constants
#define STRING_COMMIT_GROUP         "COIN_COMMITMENT_GROUP"
#define STRING_AVC_GROUP            "ACCUMULATED_VALUE_COMMITMENT_GROUP"
#define STRING_AVC_ORDER            "ACCUMULATED_VALUE_COMMITMENT_ORDER"
#define STRING_AIC_GROUP            "ACCUMULATOR_INTERNAL_COMMITMENT_GROUP"
#define STRING_QRNCOMMIT_GROUPG     "ACCUMULATOR_QRN_COMMITMENT_GROUPG"
#define STRING_QRNCOMMIT_GROUPH     "ACCUMULATOR_QRN_COMMITMENT_GROUPH"
#define ACCUMULATOR_BASE_CONSTANT   31
#define MAX_PRIMEGEN_ATTEMPTS       10000
#define MAX_ACCUMGEN_ATTEMPTS       10000
#define MAX_GENERATOR_ATTEMPTS      10000
#define NUM_SCHNORRGEN_ATTEMPTS     10000

// Prototypes
bool                primalityTestByTrialDivision(uint32_t candidate);
uint256             calculateSeed(const CBigNum& modulus, const std::string& auxString, uint32_t securityLevel, const std::string& groupName);
uint256             calculateGeneratorSeed(const uint256& seed, const uint256& pSeed, const uint256& qSeed, const std::string& label, uint32_t index, uint32_t count);

uint256             calculateHash(const uint256& input);
IntegerGroupParams  deriveIntegerGroupParams(const uint256& seed, uint32_t pLen, uint32_t qLen);
IntegerGroupParams  deriveIntegerGroupFromOrder(const CBigNum& groupOrder);
void                calculateGroupModulusAndOrder(const uint256& seed, uint32_t pLen, uint32_t qLen, CBigNum *resultModulus, CBigNum *resultGroupOrder, uint256 *resultPseed, uint256 *resultQseed);
CBigNum             calculateGroupGenerator(const uint256& seed, const uint256& pSeed, const uint256& qSeed, const CBigNum& modulus, const CBigNum& groupOrder, uint32_t index);
CBigNum             generateRandomPrime(uint32_t primeBitLen, const arith_uint256& in_seed, arith_uint256 *out_seed, uint32_t *prime_gen_counter);
CBigNum             generateIntegerFromSeed(uint32_t numBits, const arith_uint256& seed, uint32_t *numIterations);
}/* namespace libzerocoin */

#endif /* PARAMGENERATION_H_ */
