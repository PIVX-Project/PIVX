"""Regenerate the portable Draft 2020-12 report contract. No dependencies."""
import json
from pathlib import Path


def obj(**props):
    return {"type": "object", "properties": props, "required": list(props), "additionalProperties": False}


def array(items, minimum=0):
    return {"type": "array", "items": items, "minItems": minimum}


def nullable(item):
    return {"anyOf": [{"type": "null"}, item]}


text = {"type": "string", "minLength": 1}
date = {"type": "string", "format": "date-time"}
uri = {"type": "string", "format": "uri"}
amount = {"type": "string", "pattern": r"^\d+\.\d{8}$"}
positive_decimal = {"type": "string", "pattern": r"^(?:0|[1-9]\d*)(?:\.\d+)?$"}
sha = {"type": "string", "pattern": "^[a-f0-9]{64}$"}
sources = array(text, 1)
count = {"type": "integer", "minimum": 1}
score = obj(value=nullable({"type": "integer", "minimum": 0, "maximum": 5}), confidence={"enum": ["low", "medium", "high", "unscored"]}, rationale=text, source_ids=sources)
proposal = obj(
    id={"type": "string", "pattern": "^[a-zA-Z0-9-]+$"}, name=text, proposer=text,
    chain_hash=nullable(sha), chain_verified={"type": "boolean"},
    summary=obj(text=text, source_ids=sources),
    deliverables=array(obj(text=text, kind={"enum": ["ongoing", "discrete"]}, source_ids=sources), 1),
    cost=obj(cycle_piv=amount, term_piv=amount, payment_count=count, source_ids=sources,
             usd=nullable(obj(cycle_usd={"type": "string", "pattern": r"^\d+\.\d{2}$"}, usd_per_piv=positive_decimal, price_updated_at=date, source_ids=sources)),
             per_deliverable=nullable(obj(piv=amount, quantity=count, unit=text)), explanation=text),
    scores=obj(**{key: score for key in ("clarity", "cost_reasonableness", "verifiability", "track_record", "ecosystem_impact")}),
    history=obj(status={"enum": ["incomplete", "researched"]}, notes=text, source_ids=sources),
    limitations=array(obj(text=text, source_ids=sources)),
)
schema = obj(
    schema_version={"const": "0.1.0"}, kind={"const": "governance_report"}, created_at=date,
    rubric_version={"enum": ["1.0-draft", "1.0"]}, status={"enum": ["draft", "reviewed"]},
    cycle=nullable(obj(id=count, payout_height=count)),
    snapshot=obj(status={"enum": ["unverified", "partial", "verified"]}, path=nullable(text), content_sha256=nullable(sha)),
    coverage=obj(basis=text, expected_active_ids=nullable(array(text, 1)), analyzed_ids=array(text, 1)),
    disclosures=obj(status={"enum": ["pending", "confirmed"]}, text=text),
    generation=obj(mode={"enum": ["interactive_assistant", "manual_free_ui", "local_model"]}, model=text, generated_at=date, prompt_path=text, input_path=text, input_sha256=sha),
    sources=array(obj(id=text, url=uri, title=text, accessed_at=date, retrieval={"enum": ["browser_excerpt", "direct_http", "local_public_source"]}, freshness=text), 1),
    proposals=array(proposal, 1), limitations=array(text, 1),
)
schema.update({"$schema": "https://json-schema.org/draft/2020-12/schema", "title": "PIVX governance report v0.1.0"})
path = Path(__file__).resolve().parents[1] / "governance.schema.json"
path.write_text(json.dumps(schema, indent=2) + "\n")
print(path)
