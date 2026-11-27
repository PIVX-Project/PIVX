from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .contract import review_digest, validate_report
from .core import ROOT, DataError, digest, read_json, utcnow, write_json
from .ingest import collect
from .net import PublicClient
from .review import publication_errors
from .site import build_site, markdown_report
from .sources import ForumCollector, get_price


def main(argv=None):
    parser = argparse.ArgumentParser(description="Public-only PIVX governance research; local and zero-cost")
    sub = parser.add_subparsers(dest="command", required=True)
    ingest = sub.add_parser("ingest", help="Collect all public RPC providers, preserve raw bytes and emit verification status")
    ingest.add_argument("--timeout", type=float, default=8)
    ingest.add_argument("--retries", type=int, choices=(0, 1, 2), default=1)
    forum = sub.add_parser("forum", help="Fetch public threads from a snapshot, respecting robots.txt")
    forum.add_argument("snapshot", type=Path)
    history = sub.add_parser("history-inputs", help="Discover archive threads for manual six-cycle history reconstruction")
    history.add_argument("--index", default="https://forum.pivx.org/forums/archived-proposals.29/")
    history.add_argument("--pages", type=int, default=3)
    sub.add_parser("price", help="Fetch a dated free public USD quote")
    inputs = sub.add_parser("draft-inputs", help="Prepare structured input for a free UI/local model without invoking an API")
    inputs.add_argument("snapshot", type=Path)
    treasury = sub.add_parser("treasury-history", help="Verify six prior public payout windows against two explorers")
    treasury.add_argument("snapshot", type=Path)
    treasury.add_argument("--out", type=Path, default=ROOT / "data/research/treasury-history.json")
    for name in ("validate", "build", "gate", "review-template", "export"):
        cmd = sub.add_parser(name)
        cmd.add_argument("report", nargs="?", type=Path, default=ROOT / "data/drafts/initial-report.json")
        if name in ("build", "export"):
            cmd.add_argument("--out", type=Path, default=ROOT / ("publish" if name == "export" else "dist"))
        if name in ("gate", "export"):
            cmd.add_argument("--reviews", type=Path)
    args = parser.parse_args(argv)
    try:
        client = PublicClient(ROOT / "data")
        if args.command == "ingest":
            if not 0 < args.timeout <= 60:
                raise DataError("Timeout must be between 0 and 60 seconds")
            client = PublicClient(ROOT / "data", timeout=args.timeout, retries=args.retries)
            snapshot = collect(client)
            run = ROOT / "data/runs" / snapshot["started_at"].replace(":", "")
            write_json(run / "snapshot.json", snapshot)
            print(json.dumps({"status": snapshot["status"], "snapshot": str(run / "snapshot.json"), "active_proposals": len(snapshot["active_ids"]), "issues": snapshot["issues"]}, indent=2))
            return 0 if snapshot["status"] == "verified" else 2
        if args.command == "forum":
            snapshot = read_json(args.snapshot)
            collector = ForumCollector(client)
            results = []
            for p in snapshot["proposals"]:
                if p["id"] not in snapshot["active_ids"]:
                    continue
                try:
                    thread = collector.thread(p["url"])
                    thread["proposal_id"] = p["id"]
                    results.append(thread)
                except DataError as exc:
                    results.append({"proposal_id": p["id"], "url": p["url"], "error": str(exc)})
            write_json(ROOT / "data/research/forum.json", results)
            print("Saved %s thread results; errors remain explicit" % len(results))
            return 2 if not results or any("error" in r or not r["complete"] for r in results) else 0
        if args.command == "history-inputs":
            if not 1 <= args.pages <= 41:
                raise DataError("Use between 1 and 41 index pages")
            result = ForumCollector(client).index(args.index, args.pages)
            write_json(ROOT / "data/research/history-index.json", result)
            print("Discovered %s thread URLs. This is not a completed delivery audit." % len(result["threads"]))
            return 0
        if args.command == "price":
            result = get_price(client)
            write_json(ROOT / "data/research/price.json", result)
            print(json.dumps(result, indent=2))
            return 0
        if args.command == "draft-inputs":
            snapshot = read_json(args.snapshot)
            if not snapshot["proposals"]:
                raise DataError("Snapshot has no proposal inputs; do not generate an empty report")
            inputs = {"prepared_at": utcnow(), "snapshot_sha256": snapshot["content_sha256"], "snapshot_status": snapshot["status"],
                      "cycle": snapshot["cycle"], "prompt": (ROOT / "prompts/proposal-review-v1.txt").read_text(),
                      "proposals": [p for p in snapshot["proposals"] if p["id"] in snapshot["active_ids"]]}
            path = ROOT / "data/research/draft-inputs.json"
            write_json(path, inputs)
            print("Prepared %s inputs at %s; no model API invoked" % (len(inputs["proposals"]), path))
            return 0
        if args.command == "treasury-history":
            from .backfill import collect_cycles
            result = collect_cycles(client, read_json(args.snapshot), checkpoint=lambda r: write_json(args.out, r))
            write_json(args.out, result)
            print("Verified six treasury payout windows: " + str(args.out))
            return 0
        report = read_json(args.report)
        validate_report(report)
        if args.command == "validate":
            print("Valid report contract and citations: %s proposals; status %s" % (len(report["proposals"]), report["status"]))
            return 0
        if args.command == "review-template":
            package = review_digest(report)
            template = {"package_sha256": package, "correction_window": {"public_invitation_url": None, "opened_at": None, "closed_at": None},
                        "proposals": [{"proposal_id": p["id"], "package_sha256": package, "reviewer": None, "decision": "pending", "reviewed_at": None, "notes": ""} for p in report["proposals"]],
                        "evidence_checks": [{"source_id": s["id"], "result": "pending", "checked_at": None} for s in report["sources"]]}
            path = ROOT / "data/reviews" / (package[:16] + ".pending.json")
            write_json(path, template)
            print("Pending template (NOT sign-off): " + str(path))
            return 0
        if args.command in ("gate", "export"):
            reviews = read_json(args.reviews) if args.reviews else None
            errors = publication_errors(report, reviews)
            if errors:
                print(json.dumps({"publication_allowed": False, "reasons": errors}, indent=2))
                return 2
            if args.command == "gate":
                print("Publication package checks passed. No upload performed.")
                return 0
            from .archive import package_report, retain_reviewed, reviewed_packages
            out = build_site(report, args.out, published=True, history=read_json(ROOT / "data/proposers.json"), reviews=reviews, archives=reviewed_packages())
            write_json(out / "review-records.json", reviews)
            retain_reviewed(package_report(report, reviews))
            print("Reviewed static package exported locally: " + str(out))
            return 0
        if args.command == "build":
            out = build_site(report, args.out, history=read_json(ROOT / "data/proposers.json"))
            review_path = ROOT / "data/drafts/REVIEW.md"
            review_path.write_text(markdown_report(report) + "\n")
            print("Local draft: %s/index.html\nReview package: %s\nNo reviewed latest.json emitted." % (out, review_path))
            return 0
    except (DataError, OSError, KeyError, TypeError, ValueError) as exc:
        print("Error: " + str(exc), file=sys.stderr)
        return 2
    return 0
