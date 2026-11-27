from __future__ import annotations

import html
import hashlib
import json
import shutil
from decimal import Decimal
from pathlib import Path

from .contract import validate_report
from .core import DIMENSIONS, ROOT, DataError, write_json
from .history import validate_history

LABELS = {"clarity": "Clarity", "cost_reasonableness": "Cost reasonableness", "verifiability": "Verifiability", "track_record": "Track record", "ecosystem_impact": "Ecosystem impact"}


def esc(value):
    return html.escape(str(value), quote=True)


def piv(value):
    return format(Decimal(value), ",.0f")


def history_anchor(alias):
    return "proposer-" + hashlib.sha256(alias.encode()).hexdigest()[:12]


def citations(refs, sources):
    return '<span class="citations">' + " ".join('<a href="%s" title="%s">[%s]</a>' % (esc(sources[key]["url"]), esc(sources[key]["title"]), esc(key)) for key in refs) + "</span>"


def shell(title, content, relative="", *, draft=True):
    state = "Local draft · awaiting human review" if draft else "Reviewed report"
    return '''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="%s"><meta name="description" content="Public evidence, proposal costs and documented uncertainties for PIVX governance.">
<title>%s · PIVX Governance</title><link rel="stylesheet" href="%sassets/style.css"><script src="%sassets/filter.js" defer></script></head>
<body><a class="skip" href="#main">Skip to content</a><header><a class="brand" href="%sindex.html">PIVX <span>GOVERNANCE</span></a><nav aria-label="Main"><a href="%sindex.html">Proposals</a><a href="%shistory.html">History</a><a href="%sarchive/index.html">Archive</a><a href="%smethodology.html">Methodology</a></nav></header>
<div class="status">%s</div><main id="main">%s</main><footer>Independent public research · Evidence gaps remain visible · Versioned rubric and source citations</footer></body></html>''' % ("noindex,nofollow" if draft else "index,follow", esc(title), relative, relative, relative, relative, relative, relative, relative, state, content)


def proposal_body(p, sources, history_href=None):
    source_refs = citations(p["summary"]["source_ids"], sources)
    deliveries = "".join("<li>%s %s</li>" % (esc(d["text"]), citations(d["source_ids"], sources)) for d in p["deliverables"])
    scores = "".join('<tr><th scope="row">%s</th><td class="score">%s</td><td>%s <small>%s</small> %s</td></tr>' % (LABELS[key], "Unscored" if p["scores"][key]["value"] is None else str(p["scores"][key]["value"]) + " / 5", esc(p["scores"][key]["rationale"]), esc(p["scores"][key]["confidence"] + " confidence"), citations(p["scores"][key]["source_ids"], sources)) for key in DIMENSIONS)
    gaps = "".join("<li>%s %s</li>" % (esc(gap["text"]), citations(gap["source_ids"], sources)) for gap in p["limitations"])
    cost = p["cost"]
    usd = "Unavailable — no verified dated quote" if cost["usd"] is None else "$%s per cycle at %s USD/PIV (%s)" % (esc(cost["usd"]["cycle_usd"]), esc(cost["usd"]["usd_per_piv"]), esc(cost["usd"]["price_updated_at"]))
    units = "Not comparable" if cost["per_deliverable"] is None else "%s PIV per %s" % (piv(cost["per_deliverable"]["piv"]), esc(cost["per_deliverable"]["unit"]))
    body = '''<p class="eyebrow">Proposal analysis · %s</p><h1>%s</h1><p class="byline">Public alias: %s · %s</p>
<p class="summary">%s %s</p><div class="analysis-grid"><section><h2>Proposed work</h2><ul>%s</ul><h2>Evidence limits</h2><ul>%s</ul><h2>Proposer history</h2><p>%s %s</p></section>
<section><h2>Requested funding</h2><dl><dt>Per cycle</dt><dd>%s PIV</dd><dt>Full term</dt><dd>%s PIV / %s cycles</dd><dt>USD conversion</dt><dd>%s</dd><dt>Cost per deliverable</dt><dd>%s</dd></dl><p class="muted">%s %s</p></section></div>
<section><h2>Five dimensions</h2><p class="muted">Draft editorial judgments. Unscored means evidence is insufficient. No overall score.</p><div class="table-scroll"><table class="scores"><caption class="sr-only">Five scoring dimensions with confidence and evidence</caption><thead><tr><th>Dimension</th><th>Score</th><th>Rationale and evidence</th></tr></thead><tbody>%s</tbody></table></div></section>''' % (esc(p["id"]), esc(p["name"]), esc(p["proposer"]), "Chain identity verified" if p["chain_verified"] else "Chain identity unverified", esc(p["summary"]["text"]), source_refs, deliveries, gaps, esc(p["history"]["notes"]), citations(p["history"]["source_ids"], sources), piv(cost["cycle_piv"]), piv(cost["term_piv"]), cost["payment_count"], usd, units, esc(cost["explanation"]), citations(cost["source_ids"], sources), scores)
    if history_href:
        body += '<p><a href="%s#%s">Read the dated public history for %s ↗</a></p>' % (esc(history_href), history_anchor(p["proposer"]), esc(p["proposer"]))
    return body


def history_body(history, download_href="governance/proposers.json"):
    history = history or {"sources": [], "proposers": [], "cycles": []}
    validate_history(history)
    sources = {s["id"]: s for s in history.get("sources", [])}
    completed = len({c["id"] for c in history.get("cycles", []) if c.get("status") == "complete"})
    entries = sum(len(p.get("timeline", [])) for p in history.get("proposers", []))
    body = '<p class="eyebrow">Public proposer history</p><h1>Promises and records.</h1><p class="intro">Dated commitments, progress reports and public artifacts, with evidence limits kept explicit.</p><div class="report-meta"><span><strong>%s</strong> public records</span><span><strong>%s / 6</strong> payout periods researched</span></div><p class="muted">Research coverage records the stated public search, not completed delivery audits. Self-reports are claims; open code is not a merged release. Disclosed-address payments verify receipts, not personal ownership or work. The latest historical period is still in progress.</p>' % (entries, completed)
    if history.get("cycles"):
        rows = ''.join('<tr><th scope="row">%s</th><td>%s</td><td>%s</td><td>%s PIV</td><td>%s</td></tr>' % (c["id"], esc(c["payout_date"][:10]), c["payment_count"], piv(c["total_paid_piv"]), "Period finished" if c["period_finished"] else "Work period ongoing") for c in history["cycles"])
        body += '<section><h2>Treasury periods checked</h2><div class="table-scroll"><table class="periods"><thead><tr><th>Cycle</th><th>Payout date (UTC)</th><th>Outputs</th><th>Total treasury paid</th><th>Work period</th></tr></thead><tbody>%s</tbody></table></div><p>%s</p><p><a href="%s">Download search records, dates and source references ↗</a></p></section>' % (rows, esc(history.get("search_scope", {}).get("method", "See the research records for search scope.")), esc(download_href))
    for p in history.get("proposers", []):
        body += '<section id="%s"><h2>%s</h2><p>%s</p><ol class="source-list">' % (history_anchor(p["name"]), esc(p["name"]), esc(p.get("research_notes", "Historical evidence remains incomplete.")))
        for record in p.get("timeline", []):
            period = " · work period " + record["reported_period"] if record.get("reported_period") else ""
            body += '<li><small>%s · %s%s</small>%s %s</li>' % (esc(record["date"] or "Date not established"), esc(record["kind"].replace("_", " ")), esc(period), esc(record["text"]), citations(record["source_ids"], sources))
        if not p.get("timeline"):
            body += '<li>No dated records have been assembled for this alias yet.</li>'
        body += '</ol></section>'
    return body


def build_site(report, destination, *, published=False, history=None, reviews=None, archives=None):
    validate_report(report)
    if history is not None:
        validate_history(history)
    if published:
        from .review import publication_errors
        errors = publication_errors(report, reviews)
        if errors:
            raise DataError("Reviewed export refused: " + "; ".join(errors))
    destination = Path(destination)
    if destination.resolve() in (ROOT, ROOT.parent) or ROOT in destination.resolve().parents and destination.name in {"data", "analyst", "scripts", "tests", "web"}:
        raise DataError("Unsafe output directory")
    if destination.exists() and any(destination.iterdir()):
        # Own generated trees only; never recursively remove an arbitrary folder.
        if not (destination / ".analyst-build").is_file():
            raise DataError("Refusing to overwrite a directory without the build marker")
        shutil.rmtree(destination)
    destination.mkdir(parents=True, exist_ok=True)
    (destination / ".analyst-build").write_text("generated local report\n")
    (destination / "assets").mkdir()
    for filename in ("style.css", "filter.js"):
        shutil.copyfile(ROOT / "web" / filename, destination / "assets" / filename)
    sources = {s["id"]: s for s in report["sources"]}
    rows = "".join('<tr data-proposal><td><a href="proposals/%s.html">%s <span aria-hidden="true">↗</span></a><small>%s</small></td><td>%s <small>PIV / cycle</small></td><td><span class="text-state">%s</span></td></tr>' % (esc(p["id"]), esc(p["name"]), esc(p["proposer"]), piv(p["cost"]["cycle_piv"]), "Reviewed" if published else "Draft") for p in report["proposals"])
    count = len(report["proposals"])
    endpoint = "latest.json" if published else "draft.json"
    limitations = "".join("<li>%s</li>" % esc(text) for text in report["limitations"])
    content = '''<p class="eyebrow">Report prepared %s</p><h1>Governance,<br>with the evidence.</h1><p class="intro">Proposal summaries, requested funding and the limits of what public records show.</p>
<div class="report-meta"><span><strong>%s</strong> proposal %s</span><span>%s</span><a href="governance/%s">Download %s JSON ↗</a></div>
<section aria-labelledby="proposals-title"><div class="section-heading"><h2 id="proposals-title">Proposal ledger</h2><label class="filter">Find a proposal <input id="proposal-search" type="search" placeholder="Name or proposer" aria-controls="proposal-list"></label></div><p id="filter-count" class="muted" role="status" aria-live="polite">%s proposals shown</p>
<div class="table-scroll"><table id="proposal-list"><caption class="sr-only">Proposals and requested PIV funding per cycle</caption><thead><tr><th>Proposal / proposer</th><th>Requested funding</th><th>Review</th></tr></thead><tbody>%s</tbody></table></div></section>
<section class="notes"><h2>What remains uncertain</h2><p>%s</p><ul>%s</ul><p><a href="methodology.html">Read the methodology and source notes ↗</a></p></section>''' % (esc(report["created_at"][:10]), count, "analyses" if published else "drafts", "Cycle %s" % report["cycle"]["id"] if report["cycle"] else "Chain roster unverified", endpoint, "reviewed" if published else "draft", count, rows, esc(report["coverage"]["basis"]), limitations)
    (destination / "index.html").write_text(shell("Proposal ledger", content, draft=not published))
    (destination / "proposals").mkdir()
    for p in report["proposals"]:
        body = '<a class="back" href="../index.html">← All proposals</a>' + proposal_body(p, sources, "../history.html" if history else None)
        (destination / "proposals" / (p["id"] + ".html")).write_text(shell(p["name"], body, "../", draft=not published))
    source_list = "".join('<li><a href="%s">%s</a><small>%s · %s</small></li>' % (esc(s["url"]), esc(s["title"]), esc(s["retrieval"]), esc(s["freshness"])) for s in report["sources"])
    content = '<p class="eyebrow">Methodology</p><h1>Claims need sources.</h1><p class="intro">Every dimension carries a rationale. Missing evidence stays unknown.</p><h2>How to read this report</h2><p>Five dimensions, each weighted equally at 20%%, are shown individually without an overall score. A zero requires evidence; an unscored dimension indicates insufficient information.</p><p>%s</p><h2>Disclosure</h2><p>%s</p><h2>Sources and freshness</h2><ol class="source-list">%s</ol><h2>Documents</h2><ul><li><a href="RUBRIC.md">Scoring rubric</a></li><li><a href="NEUTRALITY.md">Neutrality and corrections</a></li><li><a href="HANDOFF.md">Maintainer handoff draft</a></li></ul>' % (esc(report["generation"]["model"]), esc(report["disclosures"]["text"]), source_list)
    (destination / "methodology.html").write_text(shell("Methodology", content, draft=not published))
    (destination / "history.html").write_text(shell("Public history", history_body(history), draft=not published))
    (destination / "archive").mkdir()
    archive = '<p class="eyebrow">Cycle archive</p><h1>Reviewed cycles.</h1>'
    archive += '<p class="intro">No reviewed cycles have been published. The current research is a local draft.</p>' if not published else '<p class="intro">Reviewed cycle packages retained for comparison. Review records are included with each cycle.</p>'
    if published:
        by_cycle = {p["report"]["cycle"]["id"]: p for p in (archives or [])}
        from .archive import package_report
        by_cycle[report["cycle"]["id"]] = package_report(report, reviews)
        for cycle_id, package in sorted(by_cycle.items(), reverse=True):
            old = package["report"]
            cycle_dir = destination / "cycles" / str(cycle_id)
            (cycle_dir / "proposals").mkdir(parents=True)
            old_sources = {s["id"]: s for s in old["sources"]}
            links = ''.join('<li><a href="proposals/%s.html">%s</a></li>' % (esc(p["id"]), esc(p["name"])) for p in old["proposals"])
            body = '<p class="eyebrow">Reviewed cycle %s</p><h1>Proposal analyses.</h1><ul>%s</ul><p><a href="../../governance/cycle-%s.json">Cycle JSON</a> · <a href="review-package.json">Review package and policies</a></p>' % (cycle_id, links, cycle_id)
            (cycle_dir / "index.html").write_text(shell("Cycle %s" % cycle_id, body, "../../", draft=False))
            old_history = json.loads(package["documents"]["data/proposers.json"])
            (cycle_dir / "history.html").write_text(shell("Cycle %s history" % cycle_id, history_body(old_history, "proposers.json"), "../../", draft=False))
            write_json(cycle_dir / "proposers.json", old_history)
            for p in old["proposals"]:
                body = '<a class="back" href="../index.html">← Cycle %s</a>' % cycle_id + proposal_body(p, old_sources, "../history.html")
                (cycle_dir / "proposals" / (p["id"] + ".html")).write_text(shell(p["name"], body, "../../../", draft=False))
            write_json(cycle_dir / "review-package.json", package)
            write_json(destination / "governance" / ("cycle-%s.json" % cycle_id), old)
            archive += '<p><a href="../cycles/%s/index.html">Cycle %s</a> · %s proposals</p>' % (cycle_id, cycle_id, len(old["proposals"]))
    (destination / "archive/index.html").write_text(shell("Cycle archive", archive, "../", draft=not published))
    write_json(destination / "governance" / endpoint, report)
    if history is not None:
        write_json(destination / "governance/proposers.json", history)
    if published:
        write_json(destination / "governance" / ("cycle-%s.json" % report["cycle"]["id"]), report)
    for name in ("RUBRIC.md", "NEUTRALITY.md", "HANDOFF.md", "governance.schema.json"):
        shutil.copyfile(ROOT / name, destination / name)
    (destination / ".nojekyll").touch()
    return destination


def markdown_report(report):
    sources = {s["id"]: s for s in report["sources"]}
    def refs(ids):
        return " ".join("[%s](%s)" % (key, sources[key]["url"]) for key in ids)
    lines = ["# Local proposal review package", "", "**UNREVIEWED DRAFT — not approved for publication.**", "", report["coverage"]["basis"], "", "Review the factual summaries, cost assumptions and draft scores below; send corrections and your PIVX roles, funding or proposer relationships for the disclosure. This human review is required before public release.", "", "[Open the local report](../../dist/index.html) · [Public history and search records](../proposers.json) · [Rubric](../../RUBRIC.md)", ""]
    for p in report["proposals"]:
        lines += ["## " + p["name"], "", p["summary"]["text"] + " " + refs(p["summary"]["source_ids"]), "", "Requested: %s PIV per cycle; %s PIV over %s cycles. %s" % (piv(p["cost"]["cycle_piv"]), piv(p["cost"]["term_piv"]), p["cost"]["payment_count"], refs(p["cost"]["source_ids"])), "", "Proposed work:", ""]
        lines[-2:-2] = ["Cost assumptions: " + p["cost"]["explanation"] + " " + refs(p["cost"]["source_ids"]), ""]
        if p["cost"]["usd"]:
            quote = p["cost"]["usd"]
            lines[-2:-2] = ["Indicative USD per cycle: $%s, at %s USD/PIV, quote updated %s. %s" % (quote["cycle_usd"], quote["usd_per_piv"], quote["price_updated_at"], refs(quote["source_ids"])), ""]
        lines += ["- " + d["text"] + " " + refs(d["source_ids"]) for d in p["deliverables"]]
        lines += ["", "| Dimension | Draft score | Rationale |", "| --- | --- | --- |"]
        for key in DIMENSIONS:
            score = p["scores"][key]
            lines += ["| %s | %s | %s %s |" % (LABELS[key], "Unscored" if score["value"] is None else "%s/5" % score["value"], score["rationale"], refs(score["source_ids"]))]
        lines += ["", "Evidence limits:", ""] + ["- " + x["text"] + " " + refs(x["source_ids"]) for x in p["limitations"]] + [""]
        lines += ["Public history: " + p["history"]["notes"] + " " + refs(p["history"]["source_ids"]), ""]
    return "\n".join(lines)
