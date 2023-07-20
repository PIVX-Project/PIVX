// Copyright (c) 2020 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef hemis_CYCLINGVECTOR_H
#define hemis_CYCLINGVECTOR_H

#include <sync.h>
#include <vector>

/*
 * Vector container that keeps only MAX_SIZE elements.
 * Initialized with empty objects.
 * Exposes only atomic getter and setter
 * (which cycles the vector index modulo MAX_SIZE)
 */
template <typename T>
class CyclingVector
{
private:
    mutable RecursiveMutex cs;
    unsigned int MAX_SIZE;
    std::vector<T> vec;

public:
    CyclingVector(unsigned int _MAX_SIZE, const T& defaultVal):
        MAX_SIZE(_MAX_SIZE),
        vec(_MAX_SIZE, defaultVal)
    {}

    T Get(int idx) const { LOCK(cs); return vec[idx % MAX_SIZE]; }
    void Set(int idx, const T& value) { LOCK(cs); vec[idx % MAX_SIZE] = value; }
    std::vector<T> GetCache() const { LOCK(cs); return vec; }
};

#endif // hemis_CYCLINGVECTOR_H
