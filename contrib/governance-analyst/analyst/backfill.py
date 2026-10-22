"""Public treasury-payment history; keep payments separate from delivery judgments."""
from decimal import Decimal
from datetime import datetime, timezone

from .core import CYCLE_BLOCKS, DataError, digest, integer, money, utcnow

EXPLORERS = ("https://explorer.pivx.org/api/v2", "https://computingcache.com/pivx/explorer/api/v2")


def payout_output(block, expected_piv):
    """Core pre-v6 FillBlockPayee appends the budget output to the coinstake.

    The explorer block-detail extension returns vout values in satoshis. Never
    treat the staked principal or total transaction output as a treasury payment.
    """
    transactions = [t for t in block.get("tx", []) if t.get("tx_type") == "coinstake"]
    if len(transactions) != 1 or not transactions[0].get("vout"):
        raise DataError("Expected one pre-v6 coinstake; unsupported payment shape")
    tx = transactions[0]
    output = tx["vout"][-1]
    sats = Decimal(str(output["value"]))
    if not sats.is_finite() or sats < 0 or sats != sats.to_integral_value():
        raise DataError("Explorer output must be an integer number of satoshis")
    piv = money(sats / 100_000_000)
    if piv != money(expected_piv) or len(output.get("addresses", [])) != 1:
        raise DataError("Indexed treasury total does not match final coinstake output")
    return {"txid": tx["txid"], "vout": integer(output["n"], "output index"),
            "address": output["addresses"][0], "piv": piv}


def collect_cycles(client, snapshot, checkpoint=None):
    if snapshot["status"] != "verified" or snapshot["cycle"] is None:
        raise DataError("A verified current snapshot is required to anchor historical cycles")
    first, stop = snapshot["cycle"]["id"] - 6, snapshot["cycle"]["id"]
    indexes, index_sources = [], []
    for base in EXPLORERS:
        rows, meta = client.json(base + "/analytics/treasury")
        if not isinstance(rows, list):
            raise DataError("Expected public treasury index array")
        indexes.append({r["height"]: r for r in rows if first * CYCLE_BLOCKS <= r["height"] < stop * CYCLE_BLOCKS})
        index_sources.append(meta)
    candidates = dict(indexes[0])
    index_disagreements = []
    for height in sorted(set(indexes[0]) | set(indexes[1])):
        if height not in indexes[0] or height not in indexes[1]:
            index_disagreements.append({"height": height, "missing_from": [EXPLORERS[n] for n in range(2) if height not in indexes[n]]})
        elif money(indexes[0][height]["total_paid"]) != money(indexes[1][height]["total_paid"]):
            raise DataError("Public explorers disagree on indexed payment amount")
        candidates.setdefault(height, indexes[1].get(height))
    result = {"kind": "treasury_payment_history", "started_at": utcnow(), "status": "partial",
              "current_snapshot_sha256": snapshot["content_sha256"], "index_sources": index_sources,
              "cycles": [], "issues": [], "index_disagreements": index_disagreements,
              "index_resolution": "Candidate union; verify every payment block against both providers and the first following non-budget block",
              "requests": client.events}
    for cycle_id in range(first, stop):
        start = cycle_id * CYCLE_BLOCKS
        heights = sorted(h for h in candidates if start <= h < start + CYCLE_BLOCKS)
        if not heights or heights != list(range(start, heights[-1] + 1)) or any(h - start >= 100 for h in heights):
            raise DataError("Unexpected treasury payment window; inspect index before accepting coverage")
        cycle = {"id": cycle_id, "payout_height": start, "status": "partial", "payout_date": None,
                 "period_finished": snapshot["height"] >= start + CYCLE_BLOCKS,
                 "payments": [], "evidence_urls": [m["url"] for m in index_sources]}
        for height in heights:
            blocks, sources = [], []
            for base in EXPLORERS:
                block, meta = client.json(base + "/block-detail/" + str(height))
                if not isinstance(block, dict) or block.get("height") != height:
                    raise DataError("Historical block height mismatch")
                blocks.append(block)
                sources.append(meta)
            if blocks[0]["hash"] != blocks[1]["hash"] or blocks[0]["time"] != blocks[1]["time"]:
                raise DataError("Historical block identity differs between public explorers")
            expected = candidates[height]["total_paid"]
            payments = [payout_output(block, expected) for block in blocks]
            if payments[0] != payments[1]:
                raise DataError("Historical payment outputs differ between public explorers")
            at = datetime.fromtimestamp(integer(blocks[0]["time"], "block time", 1), timezone.utc).isoformat()
            if cycle["payout_date"] is None:
                cycle["payout_date"] = at
            cycle["payments"].append({**payments[0], "height": height, "block_hash": blocks[0]["hash"], "at": at, "sources": sources})
            cycle["evidence_urls"].extend(m["url"] for m in sources)
        # Core pays the finalized budget's vector in consecutive blocks. Confirm
        # the index did not truncate it: the next block must have a normal MN
        # payment below the 10 PIV minimum proposal payment.
        terminal_blocks = []
        for base in EXPLORERS:
            block, meta = client.json(base + "/block-detail/" + str(heights[-1] + 1))
            stakes = [t for t in block.get("tx", []) if t.get("tx_type") == "coinstake"]
            if block.get("height") != heights[-1] + 1 or len(stakes) != 1 or not stakes[0].get("vout"):
                raise DataError("Cannot verify end of treasury payment window")
            last_value = Decimal(str(stakes[0]["vout"][-1]["value"])) / 100_000_000
            if not Decimal(0) < last_value < Decimal(10):
                raise DataError("Treasury index may omit a payment; terminal block is not a normal reward")
            terminal_blocks.append(block)
            cycle["evidence_urls"].append(meta["url"])
        if terminal_blocks[0]["hash"] != terminal_blocks[1]["hash"]:
            raise DataError("Treasury terminal block differs between providers")
        cycle["payment_window_end_verified"] = True
        cycle["total_paid_piv"] = money(sum(Decimal(p["piv"]) for p in cycle["payments"]))
        cycle["status"] = "payments_verified"
        result["cycles"].append(cycle)
        if checkpoint:
            checkpoint(result)
    result["status"] = "verified"
    result["finished_at"] = utcnow()
    result["content_sha256"] = digest({k: v for k, v in result.items() if k != "content_sha256"})
    return result
