from datetime import date

from .core import DataError, public_url

KINDS = {"commitment", "artifact_observed", "merged_code", "open_code", "review_observed", "self_report", "program_context", "closing_statement", "payment_observed"}


def validate_history(history):
    """Validate the evidence ledger without promoting sampled research to cycle coverage."""
    sources = history.get("sources", [])
    source_ids = [s["id"] for s in sources]
    if len(set(source_ids)) != len(source_ids):
        raise DataError("Duplicate historical source IDs")
    for source in sources:
        public_url(source["url"])
    known = set(source_ids)
    seen = set()
    aliases = set()
    for proposer in history.get("proposers", []):
        name = proposer["name"]
        if name in aliases:
            raise DataError("Duplicate proposer history alias")
        aliases.add(name)
        for record in proposer.get("timeline", []):
            if record["id"] in seen or record["proposer"] != name:
                raise DataError("Duplicate or misattributed history event")
            seen.add(record["id"])
            if record["kind"] not in KINDS or not record["text"].strip():
                raise DataError("Invalid historical evidence kind or empty statement")
            if record["date"] is not None:
                try:
                    date.fromisoformat(record["date"])
                except (ValueError, TypeError):
                    raise DataError("Invalid historical event date")
            if not record["source_ids"] or not set(record["source_ids"]) <= known:
                raise DataError("Unresolved historical evidence citation")
            if not isinstance(record["verified_cycle_ids"], list):
                raise DataError("Historical cycle attribution must be explicit")
    if history.get("status") == "complete":
        cycle_ids = [c["id"] for c in history.get("cycles", [])]
        if len(set(cycle_ids)) < 6 or len(cycle_ids) != len(set(cycle_ids)):
            raise DataError("Complete research needs six distinct payout periods")
        scope = history.get("search_scope", {})
        if not scope.get("thread_ids") or not scope.get("discovery_source_ids") or not set(scope["discovery_source_ids"]) <= known:
            raise DataError("Complete historical search needs bounded discovery provenance")
        for cycle in history["cycles"]:
            searches = cycle.get("searches", [])
            if len(searches) != len(aliases) or {s["proposer"] for s in searches} != aliases:
                raise DataError("Every period needs a search record for every proposer")
            for search in searches:
                refs = search.get("searched_source_ids", [])
                if not refs or not set(refs) <= known or search.get("delivery_result") not in {"unknown", "partly_verified", "verified"}:
                    raise DataError("Historical search needs sources and explicit delivery uncertainty")
                for match in search.get("matched_posts", []):
                    if match.get("source_id") not in refs or not match.get("post_id") or not match.get("posted_at"):
                        raise DataError("Historical post match needs source and post provenance")
                if not set(search.get("matched_payment_event_ids", [])) <= seen:
                    raise DataError("Historical payment event reference is unresolved")
    return True
