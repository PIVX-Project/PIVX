// Copyright (c) 2020-2021 The hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef OPERATIONRESULT_H
#define OPERATIONRESULT_H

#include "optional.h"
#include <string>

class OperationResult
{
private:
    bool m_res{false};
    Optional<std::string> m_error{nullopt};

public:
    OperationResult(bool _res, const std::string& _error) : m_res(_res), m_error(_error) { }
    OperationResult(bool _res) : m_res(_res) { }

    std::string getError() const { return (m_error ? *m_error : ""); }
    bool getRes() const { return m_res; }
    explicit operator bool() const { return m_res; }
};

inline OperationResult errorOut(const std::string& errorStr)
{
    return OperationResult(false, errorStr);
}


template <class T>
class CallResult : public OperationResult
{
private:
    Optional<T> m_obj_res{nullopt};
public:
    CallResult() : OperationResult(false) {}
    CallResult(T _obj) : OperationResult(true), m_obj_res(_obj) { }
    CallResult(const std::string& error) : OperationResult(false, error) { }
    const Optional<T>& getObjResult() const { return m_obj_res; }
};


#endif //OPERATIONRESULT_H
