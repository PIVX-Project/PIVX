from __future__ import annotations

from datetime import datetime, timezone

from .core import (CYCLE_BLOCKS, MAINNET_GENESIS, DataError, active_for_cycle, digest,
                   integer, next_superblock, proposal_rows, unwrap, utcnow)
from .net import PROXIES


def collect(client):
    started = utcnow()
    providers = []
    for base in PROXIES:
        provider = {"endpoint": base, "errors": [], "identity_verified": False, "freshness_verified": False}
        providers.append(provider)
        try:
            value, _ = client.rpc(base, "getblockcount", fresh=True)
            provider["height"] = integer(unwrap(value), "height", 1)
            value, _ = client.rpc(base, "getbudgetinfo", fresh=True)
            provider["proposals"] = proposal_rows(value)
            value, _ = client.rpc(base, "getbudgetprojection", fresh=True)
            provider["projection"] = proposal_rows(value)
            ids = {p["id"] for p in provider["proposals"]}
            if any(p["id"] not in ids for p in provider["projection"]):
                raise DataError("Projection includes proposals absent from budgetinfo")
        except (DataError, TypeError, KeyError) as exc:
            provider["errors"].append(str(exc))
            provider["usable"] = False
            continue
        provider["usable"] = True
        for method, params, key in (("getblockhash", 0, "genesis"), ("getblockhash", provider["height"], "tip_hash")):
            try:
                value, _ = client.rpc(base, method, params, fresh=True)
                value = unwrap(value)
                if not isinstance(value, str) or len(value) != 64:
                    raise DataError("Expected block hash")
                provider[key] = value
            except DataError as exc:
                provider["errors"].append(str(exc))
        provider["identity_verified"] = provider.get("genesis") == MAINNET_GENESIS
        if provider.get("tip_hash"):
            try:
                value, _ = client.rpc(base, "getblock", provider["tip_hash"], fresh=True)
                block = unwrap(value)
                if not isinstance(block, dict) or block.get("hash") != provider["tip_hash"] or block.get("height") != provider["height"]:
                    raise DataError("Tip block identity/height mismatch")
                block_time = integer(block.get("time"), "block time", 1)
                age = datetime.now(timezone.utc).timestamp() - block_time
                provider["tip_time"] = block_time
                provider["freshness_verified"] = -120 <= age <= 1800
                if not provider["freshness_verified"]:
                    provider["errors"].append("Tip is stale or future dated")
            except DataError as exc:
                provider["errors"].append(str(exc))
        try:
            end_height = integer(unwrap(client.rpc(base, "getblockcount", fresh=True)[0]), "end height", 1)
            provider["end_height"] = end_height
            if end_height < provider["height"] or end_height - provider["height"] > 6 or next_superblock(end_height) != next_superblock(provider["height"]):
                provider["usable"] = False
                provider["errors"].append("Snapshot crossed a cycle boundary or changed height unexpectedly; rerun")
        except DataError as exc:
            provider["usable"] = False
            provider["errors"].append(str(exc))
    usable = [p for p in providers if p.get("usable")]
    snapshot = {"schema_version": "0.1.0", "kind": "rpc_snapshot", "started_at": started,
                "finished_at": utcnow(), "network": "mainnet", "status": "blocked",
                "providers": providers, "proposals": [], "active_ids": [], "vote_details": {},
                "issues": [], "requests": client.events, "cycle": None}
    if not usable:
        snapshot["issues"].append("No proxy returned a complete structurally valid governance snapshot")
        snapshot["content_sha256"] = digest({k: v for k, v in snapshot.items() if k != "content_sha256"})
        return snapshot
    selected = max(usable, key=lambda p: (p["identity_verified"] and p["freshness_verified"], p["height"]))
    height = selected["height"]
    payout = next_superblock(height)
    active = [p for p in selected["proposals"] if active_for_cycle(p, payout)]
    snapshot.update({"status": "partial", "selected_endpoint": selected["endpoint"], "height": height,
                     "cycle": {"id": payout // CYCLE_BLOCKS, "payout_height": payout, "blocks": CYCLE_BLOCKS},
                     "proposals": selected["proposals"], "active_ids": [p["id"] for p in active],
                     "projected_ids": [p["id"] for p in selected["projection"]]})
    if not active:
        snapshot["issues"].append("Empty active roster requires independent confirmation")
    verified = [p for p in usable if p["identity_verified"] and p["freshness_verified"] and abs(p["height"] - height) <= 6 and next_superblock(p["height"]) == payout]
    if len(verified) < 2:
        snapshot["issues"].append("Need two fresh mainnet-identified providers within six blocks")
    expected = set(snapshot["active_ids"])
    for provider in verified:
        if {p["id"] for p in provider["proposals"] if active_for_cycle(p, payout)} != expected:
            snapshot["issues"].append("Providers disagree on active roster; reconcile before publishing")
        by_id = {p["id"]: p for p in provider["proposals"]}
        for proposal in active:
            other = by_id.get(proposal["id"])
            if other and any(other[key] != proposal[key] for key in ("cycle_piv", "term_piv", "block_start", "block_end", "url")):
                snapshot["issues"].append("Providers disagree on immutable proposal fields")
    for proposal in active:
        try:
            value, meta = client.rpc(selected["endpoint"], "getbudgetvotes", proposal["name"], fresh=True)
            votes = unwrap(value)
            if not isinstance(votes, list) or any(not isinstance(vote, dict) or not {"mnId", "nHash", "Vote", "nTime", "fValid"} <= set(vote) or vote["Vote"] not in ("YES", "NO", "ABSTAIN") or type(vote["fValid"]) is not bool or type(vote["nTime"]) is not int for vote in votes):
                raise DataError("Malformed budget votes")
            counts = {"yes": 0, "no": 0, "abstain": 0}
            for vote in votes:
                if vote["fValid"]:
                    counts[vote["Vote"].lower()] += 1
            if counts != proposal["votes"]:
                snapshot["issues"].append("Vote details and proposal totals disagree for %s; refresh to reconcile timing" % proposal["id"])
            snapshot["vote_details"][proposal["id"]] = {"votes": votes, "source": meta}
        except DataError as exc:
            snapshot["issues"].append("Vote detail unavailable for %s: %s" % (proposal["id"], exc))
    if not snapshot["issues"]:
        snapshot["status"] = "verified"
    snapshot["finished_at"] = utcnow()
    snapshot["content_sha256"] = digest({k: v for k, v in snapshot.items() if k != "content_sha256"})
    return snapshot
