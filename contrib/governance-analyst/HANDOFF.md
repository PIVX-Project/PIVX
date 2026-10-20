# Governance report interface proposal

This optional contribution provides a local research and review tool. It does not deploy a PIVX-owned service, modify pivx.ai, or establish official governance policy. The checked-in example is synthetic and must not be presented as a live report.

A separately reviewed static export can provide `governance/latest.json`, `governance/cycle-<n>.json`, `governance/proposers.json` and `governance.schema.json`. Proposal IDs in verified reports are on-chain hashes. Cycle numbers are mainnet payout heights divided by 43,200. Amounts are decimal strings and unknown judgments remain explicit.

Consumers should verify report status, cutoff, source provenance, coverage and review records. Draft JSON is not a reviewed endpoint. Original policies and history accompany reviewed cycle archives. A deployment, correction invitation, integration or public announcement requires a separate explicit decision; this tool performs none of those actions.
