// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The hemis Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/hemis-config.h"
#endif

#include "chainparams.h"
#include "clientversion.h"
#include "fs.h"
#include "init.h"
#include "masternodeconfig.h"
#include "noui.h"
#include "shutdown.h"
#include "util/system.h"

#include <stdio.h>

/* Introduction text for doxygen: */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 *
 * This is the developer documentation of the reference client for an experimental new digital currency called hemis (http://www.hemis.org),
 * which enables instant payments to anyone, anywhere in the world. hemis uses peer-to-peer technology to operate
 * with no central authority: managing transactions and issuing money are carried out collectively by the network.
 *
 * The software is a community-driven open source project, released under the MIT license.
 *
 * \section Navigation
 * Use the buttons <code>Namespaces</code>, <code>Classes</code> or <code>Files</code> at the top of the page to start navigating the code.
 */

void WaitForShutdown()
{
    while (!ShutdownRequested()) {
        MilliSleep(200);
    }
    Interrupt();
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
bool AppInit(int argc, char* argv[])
{
    bool fRet = false;

    //
    // Parameters
    //
    // If Qt is used, parameters/hemis.conf are parsed in qt/hemis.cpp's main()
    gArgs.ParseParameters(argc, argv);

    // Process help and version before taking care about datadir
    if (gArgs.IsArgSet("-?") || gArgs.IsArgSet("-h") || gArgs.IsArgSet("-help") || gArgs.IsArgSet("-version")) {
        std::string strUsage = PACKAGE_NAME " Daemon version " + FormatFullVersion() + "\n";

        if (gArgs.IsArgSet("-version")) {
            strUsage += LicenseInfo();
        } else {
            strUsage += "\nUsage:  hemisd [options]                     Start " PACKAGE_NAME " Daemon\n";
            strUsage += "\n" + HelpMessage(HMM_BITCOIND);
        }

        fprintf(stdout, "%s", strUsage.c_str());
        return true;
    }

    try {
        if (!CheckDataDirOption()) {
            fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", gArgs.GetArg("-datadir", "").c_str());
            return false;
        }
        try {
            gArgs.ReadConfigFile(gArgs.GetArg("-conf", hemis_CONF_FILENAME));
        } catch (const std::exception& e) {
            fprintf(stderr, "Error reading configuration file: %s\n", e.what());
            return false;
        }
        // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
        try {
            SelectParams(gArgs.GetChainName());
        } catch(const std::exception& e) {
            fprintf(stderr, "Error: %s\n", e.what());
            return false;
        }

        // Error out when loose non-argument tokens are encountered on command line
        for (int i = 1; i < argc; i++) {
            if (!IsSwitchChar(argv[i][0])) {
                fprintf(stderr, "Error: Command line contains unexpected token '%s', see hemisd -h for a list of options.\n", argv[i]);
                return false;
            }
        }

        // -server defaults to true for hemisd but not for the GUI so do this here
        gArgs.SoftSetBoolArg("-server", true);
        // Set this early so that parameter interactions go to console
        InitLogging();
        InitParameterInteraction();
        if (!AppInitBasicSetup()) {
            // UIError will have been called with detailed error, which ends up on console
            return false;
        }
        if (!AppInitParameterInteraction()) {
            // UIError will have been called with detailed error, which ends up on console
            return false;
        }
        if (!AppInitSanityChecks()) {
            // UIError will have been called with detailed error, which ends up on console
            return false;
        }

#ifndef WIN32
        if (gArgs.GetBoolArg("-daemon", false)) {
            fprintf(stdout, "hemis server starting\n");

            // Daemonize
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
                return false;
            }
            if (pid > 0) // Parent process, pid is child process id
            {
                return true;
            }
            // Child process falls through to rest of initialization

            pid_t sid = setsid();
            if (sid < 0)
                fprintf(stderr, "Error: setsid() returned %d errno %d\n", sid, errno);
        }
#endif

        // Set this early so that parameter interactions go to console
        fRet = AppInitMain();
    } catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInit()");
    }

    if (!fRet) {
        Interrupt();
    } else {
        WaitForShutdown();
    }
    Shutdown();

    return fRet;
}

int main(int argc, char* argv[])
{
#ifdef WIN32
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif
    SetupEnvironment();

    // Connect hemisd signal handlers
    noui_connect();

    return (AppInit(argc, argv) ? 0 : 1);
}
