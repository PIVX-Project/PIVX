from .util import (
    assert_equal,
    assert_greater_than_or_equal,
    assert_true,
    satoshi_round,
)

class Proposal:
    def __init__(self, name, link, cycles, payment_addr, amount_per_cycle):
        self.name = name
        self.link = link
        self.cycles = cycles
        self.paymentAddr = payment_addr
        self.amountPerCycle = amount_per_cycle
        self.feeTxId = ""
        self.proposalHash = ""

def get_proposal_obj(Name, URL, Hash, FeeHash, BlockStart, BlockEnd,
                     TotalPaymentCount, RemainingPaymentCount, PaymentAddress,
                     Ratio, Yeas, Nays, Abstains, TotalPayment, MonthlyPayment,
                     IsEstablished, IsValid, Allotted, TotalBudgetAllotted, IsInvalidReason = ""):
    obj = {}
    obj["Name"] = Name
    obj["URL"] = URL
    obj["Hash"] = Hash
    obj["FeeHash"] = FeeHash
    obj["BlockStart"] = BlockStart
    obj["BlockEnd"] = BlockEnd
    obj["TotalPaymentCount"] = TotalPaymentCount
    obj["RemainingPaymentCount"] = RemainingPaymentCount
    obj["PaymentAddress"] = PaymentAddress
    obj["Ratio"] = Ratio
    obj["Yeas"] = Yeas
    obj["Nays"] = Nays
    obj["Abstains"] = Abstains
    obj["TotalPayment"] = TotalPayment
    obj["MonthlyPayment"] = MonthlyPayment
    obj["IsEstablished"] = IsEstablished
    obj["IsValid"] = IsValid
    if IsInvalidReason != "":
        obj["IsInvalidReason"] = IsInvalidReason
    obj["Allotted"] = Allotted
    obj["TotalBudgetAllotted"] = TotalBudgetAllotted
    return obj

def get_proposal(prop, block_start, alloted, total_budget_alloted, positive_votes):
    blockEnd = block_start + prop.cycles * 145
    total_payment = prop.amountPerCycle * prop.cycles
    return get_proposal_obj(prop.name, prop.link, prop.proposalHash, prop.feeTxId, block_start,
                     blockEnd, prop.cycles, prop.cycles, prop.paymentAddr, 1,
                     positive_votes, 0, 0, satoshi_round(total_payment), satoshi_round(prop.amountPerCycle),
                     True, True, satoshi_round(alloted), satoshi_round(total_budget_alloted))

def check_mns_status_legacy(node, txhash):
    status = node.getmasternodestatus()
    assert_equal(status["txhash"], txhash)
    assert_equal(status["message"], "Masternode successfully started")

def check_mns_status(node, txhash):
    status = node.getmasternodestatus()
    assert_equal(status["proTxHash"], txhash)
    assert_equal(status["dmnstate"]["PoSePenalty"], 0)
    assert_equal(status["status"], "Ready")

def check_mn_list(node, txHashSet):
    # check masternode list from node
    mnlist = node.listmasternodes()
    assert_equal(len(mnlist), 3)
    foundHashes = set([mn["txhash"] for mn in mnlist if mn["txhash"] in txHashSet])
    assert_equal(len(foundHashes), len(txHashSet))

def check_budget_finalization_sync(nodes, votesCount, status):
    for i in range(0, len(nodes)):
        node = nodes[i]
        budFin = node.mnfinalbudget("show")
        assert_greater_than_or_equal(len(budFin), 1)
        budget = budFin[next(iter(budFin))]
        assert_equal(budget["VoteCount"], votesCount)
        assert_equal(budget["Status"], status)

def check_proposal_existence(nodes, proposalName, proposalHash):
    for node in nodes:
        proposals = node.getbudgetinfo(proposalName)
        assert(len(proposals) > 0)
        assert_equal(proposals[0]["Hash"], proposalHash)

def check_vote_existence(nodes, proposalName, mnCollateralHash, voteType, voteValid):
    for i in range(0, len(nodes)):
        node = nodes[i]
        node.syncwithvalidationinterfacequeue()
        votesInfo = node.getbudgetvotes(proposalName)
        assert(len(votesInfo) > 0)
        found = False
        for voteInfo in votesInfo:
            if (voteInfo["mnId"].split("-")[0] == mnCollateralHash) :
                assert_equal(voteInfo["Vote"], voteType)
                assert_equal(voteInfo["fValid"], voteValid)
                found = True
        assert_true(found, "Error checking vote existence in node " + str(i))

def check_budgetprojection(nodes, expected, log):
    for i in range(len(nodes)):
        assert_equal(nodes[i].getbudgetprojection(), expected)
        log.info("Budget projection valid for node %d" % i)

def create_proposals_tx(miner, props):
    nextSuperBlockHeight = miner.getnextsuperblock()
    for entry in props:
        proposalFeeTxId = miner.preparebudget(
            entry.name,
            entry.link,
            entry.cycles,
            nextSuperBlockHeight,
            entry.paymentAddr,
            entry.amountPerCycle)
        entry.feeTxId = proposalFeeTxId
    return props

def propagate_proposals(miner, props):
    nextSuperBlockHeight = miner.getnextsuperblock()
    for entry in props:
        proposalHash = miner.submitbudget(
            entry.name,
            entry.link,
            entry.cycles,
            nextSuperBlockHeight,
            entry.paymentAddr,
            entry.amountPerCycle,
            entry.feeTxId)
        entry.proposalHash = proposalHash
    return props
