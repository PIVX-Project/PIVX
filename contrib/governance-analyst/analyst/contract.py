from __future__ import annotations

import math
import re

from .core import DIMENSIONS, ROOT, DataError, digest, public_url, read_json, timestamp


def validate_schema(value, schema, root=None, path="$"):
    """Offline validator for the exact JSON Schema vocabulary used by our contract.

    Not a general JSON Schema implementation. Unknown keywords fail closed.
    The exported schema is also usable by a full Draft 2020-12 validator.
    """
    root = root or schema
    supported = {"$schema", "$id", "$defs", "$ref", "title", "description", "type", "properties", "required", "additionalProperties", "items", "minItems", "enum", "const", "pattern", "minimum", "maximum", "minLength", "anyOf", "format"}
    unknown = set(schema) - supported
    if unknown:
        raise DataError("Unsupported schema keywords: %s" % sorted(unknown))
    if "$ref" in schema:
        target = root
        if not schema["$ref"].startswith("#/"):
            raise DataError("Only local schema references are supported")
        for key in schema["$ref"][2:].split("/"):
            target = target[key]
        validate_schema(value, target, root, path)
        return
    if "anyOf" in schema:
        for option in schema["anyOf"]:
            try:
                validate_schema(value, option, root, path)
                break
            except DataError:
                pass
        else:
            raise DataError("%s: no anyOf branch matched" % path)
    types = schema.get("type", [])
    if isinstance(types, str):
        types = [types]
    matches = {"null": value is None, "boolean": type(value) is bool, "integer": type(value) is int,
               "number": type(value) in (int, float) and math.isfinite(value), "string": isinstance(value, str),
               "object": isinstance(value, dict), "array": isinstance(value, list)}
    if types and not any(matches.get(t, False) for t in types):
        raise DataError("%s: expected %s" % (path, types))
    if "enum" in schema and value not in schema["enum"]:
        raise DataError("%s: invalid enum" % path)
    if "const" in schema and value != schema["const"]:
        raise DataError("%s: invalid constant" % path)
    if isinstance(value, dict):
        props = schema.get("properties", {})
        missing = set(schema.get("required", [])) - set(value)
        if missing:
            raise DataError("%s: missing %s" % (path, sorted(missing)))
        extra = set(value) - set(props)
        if schema.get("additionalProperties") is False and extra:
            raise DataError("%s: unexpected properties %s" % (path, sorted(extra)))
        for key, item in value.items():
            if key in props:
                validate_schema(item, props[key], root, path + "." + key)
    if isinstance(value, list):
        if len(value) < schema.get("minItems", 0):
            raise DataError("%s: too few items" % path)
        for index, item in enumerate(value):
            if "items" in schema:
                validate_schema(item, schema["items"], root, "%s[%s]" % (path, index))
    if isinstance(value, str):
        if len(value) < schema.get("minLength", 0) or ("pattern" in schema and not re.search(schema["pattern"], value)):
            raise DataError("%s: invalid string" % path)
        if schema.get("format") == "date-time":
            timestamp(value)
        elif schema.get("format") == "uri":
            public_url(value)
        elif "format" in schema and schema["format"] not in ("date-time", "uri"):
            raise DataError("Unknown format")
    if type(value) in (int, float):
        if not math.isfinite(value) or value < schema.get("minimum", -math.inf) or value > schema.get("maximum", math.inf):
            raise DataError("%s: number outside bounds" % path)


def validate_report(report):
    validate_schema(report, read_json(ROOT / "governance.schema.json"))
    source_ids = [source["id"] for source in report["sources"]]
    ids = [p["id"] for p in report["proposals"]]
    if len(set(source_ids)) != len(source_ids) or len(set(ids)) != len(ids):
        raise DataError("Duplicate source or proposal id")
    known = set(source_ids)
    def refs(refs):
        if not refs or not set(refs) <= known:
            raise DataError("Missing or unresolved source citations")
    for p in report["proposals"]:
        if len(p["summary"]["text"].split()) > 150:
            raise DataError("Summary exceeds 150 words")
        refs(p["summary"]["source_ids"])
        if set(p["scores"]) != set(DIMENSIONS):
            raise DataError("Exactly five scoring dimensions required")
        for score in p["scores"].values():
            refs(score["source_ids"])
            if score["value"] is None and score["confidence"] != "unscored":
                raise DataError("Unknown score must have unscored confidence")
            if score["value"] is not None and score["confidence"] == "unscored":
                raise DataError("Scored dimension needs confidence")
        for d in p["deliverables"]:
            refs(d["source_ids"])
        refs(p["cost"]["source_ids"])
        from decimal import Decimal
        if Decimal(p["cost"]["cycle_piv"]) * p["cost"]["payment_count"] != Decimal(p["cost"]["term_piv"]):
            raise DataError("Report full-term cost is inconsistent")
        refs(p["history"]["source_ids"])
        for limitation in p["limitations"]:
            refs(limitation["source_ids"])
        if p["cost"]["usd"] is not None:
            from decimal import Decimal
            usd = p["cost"]["usd"]
            refs(usd["source_ids"])
            if Decimal(usd["usd_per_piv"]) <= 0:
                raise DataError("USD conversion needs a positive quote")
            if Decimal(usd["cycle_usd"]) != (Decimal(p["cost"]["cycle_piv"]) * Decimal(usd["usd_per_piv"])).quantize(Decimal("0.01")):
                raise DataError("USD conversion does not match cycle amount")
        if p["cost"]["per_deliverable"] is not None:
            from decimal import Decimal
            unit = p["cost"]["per_deliverable"]
            if Decimal(unit["piv"]) != (Decimal(p["cost"]["cycle_piv"]) / unit["quantity"]).quantize(Decimal("0.00000001")):
                raise DataError("Cost per deliverable does not match denominator")
    if set(report["coverage"]["analyzed_ids"]) != set(ids) or len(report["coverage"]["analyzed_ids"]) != len(ids):
        raise DataError("Analyzed coverage does not match report")
    expected = report["coverage"]["expected_active_ids"]
    if expected is not None and (len(set(expected)) != len(expected) or set(expected) != set(ids)):
        raise DataError("Report does not cover exactly the confirmed active roster")
    if report["cycle"] is not None and report["cycle"]["payout_height"] != report["cycle"]["id"] * 43200:
        raise DataError("Cycle id must equal payout height divided by 43200")
    return True


def review_digest(report):
    return digest({"report": report, "documents": review_documents(report)})


def review_documents(report):
    names = ("RUBRIC.md", "NEUTRALITY.md", "data/proposers.json", report["generation"]["prompt_path"], report["generation"]["input_path"])
    documents = {}
    for name in names:
        path = (ROOT / name).resolve()
        if ROOT.resolve() not in path.parents:
            raise DataError("Review documents must stay inside the project")
        documents[name] = path.read_text()
    return documents
