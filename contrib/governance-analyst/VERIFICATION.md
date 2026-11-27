# Contribution verification

The contribution uses synthetic checked-in examples so reviewers can run all tests and inspect the interface without fetching public data or publishing unreviewed real-proposer reports. Tests do not require network access, credentials, a node, a wallet or a model API.

The Python suite exercises exact cycle boundaries and decimal costs, eligibility including failing/unestablished proposals, fallback, mainnet identity/freshness, vote mismatches, raw/cache integrity, malformed HTTP responses, request spacing, bounded redirects, robots policy, XHTML/quote/post-date extraction, missing/stale price rejection, contract semantics, historical-search provenance, treasury index gaps and satoshi units. Synthetic integration tests exercise reviewed export, rejection of pending/stale approvals and two-cycle archive preservation. Synthetic approvals stay in temporary test directories.

The two Node tests exercise filtering, empty results/reset and shared script loading on detail pages. Optional `scripts/check_browser.py` checks the synthetic build with independent Draft 2020-12 validation and real Chrome at desktop/mobile widths, including search, no document overflow, local links/anchors and absence of reviewed latest JSON.

These checks do not establish production delivery, official policy acceptance, live-service availability or Core binary correctness. This optional tool does not change Core consensus, wallet or RPC implementation.

Verified on 2026-10-20T04:41:38.482273+00:00: 29 Python tests and two Node tests passed; the report contract and build passed; independent Draft 2020-12 validation passed; Chrome passed 20 desktop/mobile page checks with no document overflow or page JavaScript errors; all 104 local links/anchors resolved. The default reviewed export was not produced.
