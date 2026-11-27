from __future__ import annotations

import hashlib
from datetime import datetime, timezone
from pathlib import Path

from .contract import review_digest, validate_report
from .core import ROOT, DataError, digest, next_superblock, public_url, read_json, timestamp
from .history import validate_history


def local_path(relative):
    root = ROOT.resolve()
    path = (root / relative).resolve()
    if root not in path.parents:
        raise DataError("Artifact path must stay inside the project")
    return path


def publication_errors(report, reviews=None):
    errors = []
    def check_raw(value):
        if isinstance(value, dict):
            if "body_path" in value and "sha256" in value:
                if hashlib.sha256(local_path("data/" + value["body_path"]).read_bytes()).hexdigest() != value["sha256"]:
                    errors.append("Research raw source checksum mismatch")
            for item in value.values():
                check_raw(item)
        elif isinstance(value, list):
            for item in value:
                check_raw(item)
    try:
        validate_report(report)
    except (DataError, KeyError, TypeError) as exc:
        return [str(exc)]
    if report["status"] != "reviewed":
        errors.append("Report is not marked reviewed")
    if report["disclosures"]["status"] != "confirmed":
        errors.append("Michael's affiliation disclosure is pending")
    if report["cycle"] is None:
        errors.append("Cycle membership has not been verified")
    if report["snapshot"]["status"] != "verified" or not report["snapshot"]["path"]:
        errors.append("A verified public RPC snapshot is required")
    else:
        try:
            snapshot = read_json(local_path(report["snapshot"]["path"]))
            actual = digest({key: value for key, value in snapshot.items() if key != "content_sha256"})
            if actual != snapshot["content_sha256"] or actual != report["snapshot"]["content_sha256"] or snapshot["status"] != "verified":
                errors.append("RPC snapshot content or status does not match report")
            if snapshot["issues"]:
                errors.append("RPC snapshot has unresolved issues")
            age = (datetime.now(timezone.utc) - timestamp(snapshot["finished_at"])).total_seconds()
            if not 0 <= age <= 21600:
                errors.append("RPC snapshot must be less than six hours old")
            if snapshot["cycle"] is None or report["cycle"] != {key: snapshot["cycle"][key] for key in ("id", "payout_height")}:
                errors.append("Report and snapshot cycles differ")
            if next_superblock(snapshot["height"]) != snapshot["cycle"]["payout_height"]:
                errors.append("Snapshot cycle math is inconsistent")
            if set(snapshot["active_ids"]) != set(report["coverage"]["expected_active_ids"] or []):
                errors.append("Confirmed active roster differs from snapshot")
            by_id = {p["id"]: p for p in snapshot["proposals"]}
            for proposal in report["proposals"]:
                normalized = by_id.get(proposal["id"])
                if normalized is None or proposal["name"] != normalized["name"] or any(proposal["cost"][key] != normalized[key] for key in ("cycle_piv", "term_piv", "payment_count")):
                    errors.append("Report identity or cost differs from raw proposal: " + proposal["id"])
            candidates = [p for p in snapshot["providers"] if p.get("usable") and p["identity_verified"] and p["freshness_verified"] and abs(p["height"] - snapshot["height"]) <= 6]
            if len({p["endpoint"] for p in candidates}) < 2:
                errors.append("Snapshot needs two independent fresh mainnet providers")
            for request in snapshot["requests"]:
                if "sha256" in request:
                    path = local_path("data/" + request["body_path"])
                    if hashlib.sha256(path.read_bytes()).hexdigest() != request["sha256"]:
                        errors.append("Raw source snapshot checksum mismatch")
        except (OSError, DataError, KeyError, TypeError, ValueError) as exc:
            errors.append("Cannot verify snapshot: %s" % exc)
    for proposal in report["proposals"]:
        if not proposal["chain_verified"] or proposal["chain_hash"] != proposal["id"]:
            errors.append("Proposal identity unverified: " + proposal["id"])
        if proposal["history"]["status"] != "researched":
            errors.append("Proposer history incomplete: " + proposal["proposer"])
    try:
        inputs = read_json(local_path(report["generation"]["input_path"]))
        if digest(inputs) != report["generation"]["input_sha256"]:
            errors.append("Generation inputs changed since drafting")
        if not local_path(report["generation"]["prompt_path"]).is_file():
            errors.append("Generation prompt is missing")
        check_raw(inputs)
    except (OSError, DataError, ValueError):
        errors.append("Generation input/prompt provenance cannot be verified")
    history = read_json(ROOT / "data/proposers.json")
    try:
        validate_history(history)
        artifact = history.get("treasury_artifact", {})
        treasury = read_json(local_path(artifact["path"]))
        treasury_hash = digest({k: v for k, v in treasury.items() if k != "content_sha256"})
        if treasury["status"] != "verified" or treasury_hash != artifact["content_sha256"] or treasury_hash != treasury["content_sha256"]:
            errors.append("Historical treasury artifact changed or is not verified")
        if [c["id"] for c in treasury["cycles"]] != [c["id"] for c in history["cycles"]]:
            errors.append("Historical search periods differ from verified payment periods")
        check_raw(treasury)
    except (OSError, DataError, ValueError, KeyError, TypeError) as exc:
        errors.append("Historical evidence ledger is invalid: %s" % exc)
    complete = [cycle for cycle in history["cycles"] if cycle.get("status") == "complete"]
    cycle_ids = [cycle.get("id") for cycle in complete]
    if history["status"] != "complete" or len(set(cycle_ids)) < 6 or any(type(n) is not int or n <= 0 for n in cycle_ids):
        errors.append("Six historical cycles have not been reconstructed")
    elif report["cycle"] and set(range(report["cycle"]["id"] - 6, report["cycle"]["id"])) - set(cycle_ids):
        errors.append("History must cover the six cycles immediately before this report")
    for cycle in complete:
        if not cycle.get("evidence_urls"):
            errors.append("Historical cycle needs public search/evidence provenance")
        for url in cycle.get("evidence_urls", []):
            try:
                public_url(url)
            except DataError:
                errors.append("Invalid public historical source")
    by_proposer = {p["name"]: p for p in history["proposers"]}
    for proposal in report["proposals"]:
        entry = by_proposer.get(proposal["proposer"], {})
        if len(set(entry.get("complete_cycle_ids", [])) & set(cycle_ids)) < 6:
            errors.append("Six-cycle search record missing for " + proposal["proposer"])
    if not reviews:
        errors.append("Michael's explicit review records and public correction-window record are missing")
        return errors
    package_hash = review_digest(report)
    if reviews.get("package_sha256") != package_hash:
        errors.append("Review package hash is stale or missing")
    try:
        window = reviews["correction_window"]
        public_url(window["public_invitation_url"])
        start, end = timestamp(window["opened_at"]), timestamp(window["closed_at"])
        if (end - start).total_seconds() < 172800 or datetime.now(timezone.utc) < end:
            errors.append("The public 48-hour correction window has not finished")
    except (DataError, KeyError, TypeError, ValueError):
        errors.append("A real public invitation and completed 48-hour window are required")
    records = reviews.get("proposals", [])
    ids = [r.get("proposal_id") for r in records]
    if len(set(ids)) != len(ids) or set(ids) != {p["id"] for p in report["proposals"]}:
        errors.append("One explicit review record is required for every proposal")
    for record in records:
        if record.get("reviewer") != "Michael Korovkin" or record.get("decision") != "approved" or record.get("package_sha256") != package_hash:
            errors.append("Missing approval for " + str(record.get("proposal_id")))
        try:
            reviewed = timestamp(record["reviewed_at"])
            if reviewed > datetime.now(timezone.utc) or reviewed < timestamp(reviews["correction_window"]["closed_at"]):
                errors.append("Final sign-off must follow the correction window")
        except (KeyError, ValueError, TypeError, DataError):
            errors.append("Missing or invalid human review timestamp")
    checked = {r.get("source_id") for r in reviews.get("evidence_checks", []) if r.get("result") == "checked"}
    if checked != {s["id"] for s in report["sources"]}:
        errors.append("Public evidence links need recorded checks or removal from the report")
    return errors
