// Copyright (c) 2015-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include "lightzpivthread.h"
#include "validation.h"

/****** Thread ********/
void CLightWorker::ThreadLightZPIVSimplified() {
    RenameThread("pivx-light-thread");
    isWorkerRunning = true;
    while (true) {
        try {

            // Take a breath between requests.. TODO: Add processor usage check here
            MilliSleep(2000);

            // TODO: Future: join several similar requests into one calculation if the filter and denom match..
            CGenWit genWit = requestsQueue.pop();
            LogPrintf("%s pop work for %s \n\n", "pivx-light-thread", genWit.toString());

            libzerocoin::ZerocoinParams *params = Params().Zerocoin_Params(false);
            CBlockIndex *pIndex = chainActive[genWit.getStartingHeight()];
            if (!pIndex) {
                // Rejects only the failed height
                rejectWork(genWit, genWit.getStartingHeight(), NON_DETERMINED);
            } else {
                LogPrintf("%s calculating work for %s \n\n", "pivx-light-thread", genWit.toString());
                int blockHeight = pIndex->nHeight;
                if (blockHeight >= Params().Zerocoin_Block_V2_Start()) {

                    // TODO: The protocol actually doesn't care about the Accumulator..
                    libzerocoin::Accumulator accumulator(params, genWit.getDen(), genWit.getAccWitValue());
                    libzerocoin::PublicCoin temp(params);
                    libzerocoin::AccumulatorWitness witness(params, accumulator, temp);
                    std::string strFailReason = "";
                    int nMintsAdded = 0;
                    CZerocoinSpendReceipt receipt;

                    std::list<CBigNum> ret;
                    int heightStop;

                    bool res;
                    try {

                        res = CalculateAccumulatorWitnessFor(
                                params,
                                blockHeight,
                                COMP_MAX_AMOUNT,
                                genWit.getDen(),
                                genWit.getFilter(),
                                accumulator,
                                witness,
                                nMintsAdded,
                                strFailReason,
                                ret,
                                heightStop
                        );

                    } catch (NotEnoughMintsException e) {
                        LogPrintStr(std::string("ThreadLightZPIVSimplified: ") + e.message + "\n");
                        rejectWork(genWit, blockHeight, NOT_ENOUGH_MINTS);
                        continue;
                    }

                    if (!res) {
                        // TODO: Check if the GenerateAccumulatorWitnessFor can fail for node's fault or it's just because the peer sent an illegal request..
                        rejectWork(genWit, blockHeight, NON_DETERMINED);
                    } else {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(ret.size() * 32);

                        ss << genWit.getRequestNum();
                        ss << accumulator.getValue(); // TODO: ---> this accumulator value is not necessary. The light node should get it using the other message..
                        ss << witness.getValue();
                        uint32_t size = ret.size();
                        ss << size;
                        for (CBigNum bnValue : ret) {
                            ss << bnValue;
                        }
                        ss << heightStop;
                        if (genWit.getPfrom()) {
                            LogPrintf("%s pushing message to %s \n", "pivx-light-thread", genWit.getPfrom()->addrName);
                            genWit.getPfrom()->PushMessage("pubcoins", ss);
                        } else
                            LogPrintf("%s NOT pushing message to %s \n", "pivx-light-thread", genWit.getPfrom()->addrName);
                    }
                } else {
                    // Rejects only the failed height
                    rejectWork(genWit, blockHeight, NON_DETERMINED);
                }
            }
        } catch (std::exception& e) {
            //std::cout << "exception in light loop, closing it. " << e.what() << std::endl;
            PrintExceptionContinue(&e, "lightzpivthread");
            break;
        }
    }


}

// TODO: Think more the peer misbehaving policy..
void CLightWorker::rejectWork(CGenWit& wit, int blockHeight, uint32_t errorNumber) {
    if (wit.getStartingHeight() == blockHeight){
        LogPrintf("%s rejecting work %s , error code: %s\n", "pivx-light-thread", wit.toString(), errorNumber);
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << wit.getRequestNum();
        ss << errorNumber;
        wit.getPfrom()->PushMessage("pubcoins", ss);
    } else {
        requestsQueue.push(wit);
    }
}
