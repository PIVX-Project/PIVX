// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_BUDGET_BUDGETDB_H
#define PIVX_BUDGET_BUDGETDB_H

#include "budget/budgetmanager.h"
#include "fs.h"

void DumpBudgets(CBudgetManager& budgetman);


/** Save Budget Manager (budget.dat)
 */
class CBudgetDB
{
private:
    fs::path pathDB;
    std::string strMagicMessage;

public:
    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    CBudgetDB();
    bool Write(const CBudgetManager& objToSave);
    ReadResult Read(CBudgetManager& objToLoad, bool fDryRun = false);
};

#endif // PIVX_BUDGET_BUDGETDB_H
