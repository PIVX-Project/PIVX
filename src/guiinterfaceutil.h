//Copyright (c) 2020 The PIVX developers
//Copyright (c) 2020 The Sombe developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GUIINTERFACEUTIL_H
#define GUIINTERFACEUTIL_H

inline static bool UIError(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "Error", CClientUIInterface::MSG_ERROR);
    return false;
}

inline static bool UIWarning(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "Warning", CClientUIInterface::MSG_WARNING);
    return true;
}

inline static std::string AmountErrMsg(const char * const optname, const std::string& strValue)
{
    return strprintf(_("Invalid amount for -%s=<amount>: '%s'"), optname, strValue);
}

#endif //GUIINTERFACEUTIL_H
