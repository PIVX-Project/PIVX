# Governance analyst prototype

An optional Python tool for preparing cited PIVX governance reports from public, read-only sources. It runs separately from PIVX Core and does not use wallets, submit votes, operate a node, invoke paid model APIs or publish reports automatically. Python 3.9+ and its standard library are sufficient for runtime.

The checked-in data is entirely synthetic: six invented proposals and a bounded historical-search example. It demonstrates the interface and exercises offline tests; it is not a report about real proposers. Default examples are unverified, unreviewed and refused by the publication gate. Real research snapshots, forum bodies, proposer scores and reviewer records are excluded from this contribution.

## Run locally

From the PIVX repository root:

```sh
cd contrib/governance-analyst
python3 -m analyst validate
python3 -m analyst build
```

Open `dist/index.html` to inspect the local synthetic preview. Generated pages have relative links and no external fonts, scripts or analytics. Draft builds emit `governance/draft.json`; only reviewed exports may emit `governance/latest.json` and cycle JSON. Generated `dist/`, raw network data, caches and review artifacts are ignored by Git.

## Collect public evidence

```sh
python3 -m analyst ingest
python3 -m analyst forum data/runs/<run-id>/snapshot.json
python3 -m analyst price
python3 -m analyst history-inputs --pages 3
python3 -m analyst treasury-history data/runs/<run-id>/snapshot.json --out data/research/treasury-history.json
python3 -m analyst draft-inputs data/runs/<run-id>/snapshot.json
```

The collector uses three named community RPC proxies with bounded retries, caching, retained response bytes and retrieval/checksum metadata, and at most one request per second per collector. It validates mainnet genesis, tip freshness, provider agreement, proposal amounts and vote totals. Mainnet's strictly-next payout boundary is `height - height % 43200 + 43200`, including at an exact boundary. Proposal eligibility includes valid objects spanning the next full period regardless of whether the funding projection includes them. Collection failures remain explicit and return exit status 2.

Forum extraction checks robots.txt, follows bounded public pagination, separates quoted text, preserves original post dates and ignores source text as instructions. History reconstruction remains a curated task; discovery is not a delivery audit. The treasury collector cross-checks six previous payment windows against two explorers, reconciles missing index candidates against actual blocks, and distinguishes coinstake budget outputs in satoshis from staking principal. Unsupported payment shapes fail closed.

Public receipts, code artifacts, proposer self-reports and independent delivery verification are separate facts. A missing receipt match does not establish nonpayment. USD conversions need a dated positive quote and exact arithmetic; cost-per-deliverable stays unknown without a comparable denominator. Five editorial dimensions have explicit rationales and confidence; unknown is not zero. No overall score or voting recommendation is produced.

## Drafting and review

Use the versioned prompt in `prompts/`, retain actual inputs and generation provenance, then edit the structured report manually. No inference service is invoked by this tool. The prototype's single-reviewer policy names Michael Korovkin; adapting that policy to another maintainer is future integration work. The rubric, neutrality policy and human-review checks are proposals for review, not an official DAO policy.

```sh
python3 -m analyst review-template path/to/report.json
python3 -m analyst gate path/to/report.json --reviews path/to/reviews.json
python3 -m analyst export path/to/report.json --reviews path/to/reviews.json
```

Pending templates are not approvals. Reviewed export requires a fresh verified chain snapshot, matching coverage/costs, complete bounded historical search records, retained source integrity, disclosure, an authorized public invitation and completed 48-hour correction window, then explicit per-proposal approval. The package hash binds the report, policies, history, prompt and generation inputs. Changes invalidate approval. Export writes local files only; it does not upload or send messages. These checks prevent accidental release through the CLI and cannot stop manual uploading of arbitrary files.

Collection does not automatically replace the checked-in synthetic example. Prepare and review a real report separately. Historical source coverage must be stated honestly and unknown delivery results retained. See [RUBRIC.md](RUBRIC.md), [NEUTRALITY.md](NEUTRALITY.md), [HANDOFF.md](HANDOFF.md) and [VERIFICATION.md](VERIFICATION.md).

## Validation

```sh
python3 -m unittest discover -s tests -v
node --test tests/filter.test.cjs
python3 -m analyst validate
python3 -m analyst build
```

Node is needed only for two optional interaction tests. The runtime contract checker supports the exported schema's exact vocabulary and fails closed on unsupported keywords. Optional independent Draft 2020-12 and actual Chrome desktop/mobile checks use the development dependencies:

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
python3 -m http.server 8768 --bind 127.0.0.1 --directory dist
```

In another terminal, from this same directory, run `.venv/bin/python scripts/check_browser.py`. This requires installed Google Chrome. No Core binary build is needed for the optional tool's tests.
