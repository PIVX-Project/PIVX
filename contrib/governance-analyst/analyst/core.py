from __future__ import annotations

import hashlib
import ipaddress
import json
import re
from datetime import datetime, timezone
from decimal import Decimal, InvalidOperation
from pathlib import Path
from urllib.parse import urlsplit

CYCLE_BLOCKS = 43200
MAINNET_GENESIS = "0000041e482b9b9691d98eefb48473405c0b8ec31b76df3797c74a78680ef818"
DIMENSIONS = ("clarity", "cost_reasonableness", "verifiability", "track_record", "ecosystem_impact")
ROOT = Path(__file__).resolve().parents[1]


class DataError(ValueError):
    pass


def utcnow():
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def timestamp(value):
    if not isinstance(value, str):
        raise DataError("Timestamp must be a string with a timezone")
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None:
        raise DataError("Timestamp must include a timezone")
    return parsed


def read_json(path):
    return json.loads(Path(path).read_text())


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + "\n")
    tmp.replace(path)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode()).hexdigest()


def next_superblock(height, cycle_blocks=CYCLE_BLOCKS):
    if type(height) is not int or height < 0 or type(cycle_blocks) is not int or cycle_blocks <= 0:
        raise DataError("Nonnegative integer height and positive integer cycle length required")
    return height - height % cycle_blocks + cycle_blocks


def money(value):
    try:
        if isinstance(value, bool):
            raise InvalidOperation
        amount = Decimal(str(value))
        if not amount.is_finite() or amount < 0 or amount.as_tuple().exponent < -8:
            raise InvalidOperation
        return format(amount, ".8f")
    except (InvalidOperation, TypeError, ValueError):
        raise DataError("Invalid PIV amount: %r" % value)


def public_url(url):
    if not isinstance(url, str):
        raise DataError("Public HTTPS URL must be a string")
    parts = urlsplit(url)
    if parts.scheme != "https" or not parts.hostname or parts.username or parts.password or parts.port not in (None, 443):
        raise DataError("Public HTTPS URL required")
    if parts.hostname == "localhost" or "." not in parts.hostname or parts.hostname.endswith((".local", ".internal")):
        raise DataError("Public hostname required")
    try:
        address = ipaddress.ip_address(parts.hostname)
    except ValueError:
        address = None
    if address is not None and not address.is_global:
        raise DataError("Public IP address required")
    return url


def unwrap(value):
    if isinstance(value, dict) and "error" in value and value["error"] is not None:
        raise DataError("RPC error: %s" % value["error"])
    if isinstance(value, dict) and "result" in value:
        return value["result"]
    return value


def integer(value, name, minimum=0):
    if type(value) is not int or value < minimum:
        raise DataError("%s must be an integer >= %s" % (name, minimum))
    return value


def proposal_rows(value):
    value = unwrap(value)
    if not isinstance(value, list):
        raise DataError("Expected proposal array")
    seen = set()
    result = []
    for item in value:
        if not isinstance(item, dict):
            raise DataError("Proposal must be an object")
        p = normalize_proposal(item)
        if p["id"] in seen:
            raise DataError("Duplicate proposal hash")
        seen.add(p["id"])
        result.append(p)
    return result


def normalize_proposal(item):
    required = ("Name", "URL", "Hash", "FeeHash", "BlockStart", "BlockEnd", "TotalPaymentCount", "RemainingPaymentCount", "PaymentAddress", "Yeas", "Nays", "Abstains", "MonthlyPayment", "TotalPayment", "IsValid", "IsEstablished", "Allotted")
    if any(key not in item for key in required):
        raise DataError("Missing Core proposal fields")
    for key in ("Hash", "FeeHash"):
        if not isinstance(item[key], str) or not re.fullmatch(r"[a-fA-F0-9]{64}", item[key]):
            raise DataError("Invalid %s" % key)
    for key in ("Name", "PaymentAddress"):
        if not isinstance(item[key], str) or not item[key].strip():
            raise DataError("Missing %s" % key)
    for key in ("IsValid", "IsEstablished"):
        if type(item[key]) is not bool:
            raise DataError("%s must be boolean" % key)
    for key in ("BlockStart", "BlockEnd", "TotalPaymentCount", "RemainingPaymentCount", "Yeas", "Nays", "Abstains"):
        integer(item[key], key)
    if item["BlockEnd"] < item["BlockStart"] or item["RemainingPaymentCount"] > item["TotalPaymentCount"]:
        raise DataError("Inconsistent proposal term")
    amount = money(item["MonthlyPayment"])
    total = money(item["TotalPayment"])
    if Decimal(amount) * item["TotalPaymentCount"] != Decimal(total):
        raise DataError("Term payment total does not match cycle amount")
    return {
        "id": item["Hash"].lower(), "name": item["Name"], "url": public_url(item["URL"]),
        "fee_hash": item["FeeHash"].lower(), "block_start": item["BlockStart"], "block_end": item["BlockEnd"],
        "payment_count": item["TotalPaymentCount"], "remaining_payments": item["RemainingPaymentCount"],
        "payment_address": item["PaymentAddress"], "cycle_piv": amount, "term_piv": total,
        "votes": {"yes": item["Yeas"], "no": item["Nays"], "abstain": item["Abstains"]},
        "allotted_piv": money(item["Allotted"]), "valid": item["IsValid"], "established": item["IsEstablished"],
    }


def active_for_cycle(proposal, payout_height):
    # Mirrors Core's IsPassing term check; do not filter by vote count or establishment.
    return proposal["valid"] and proposal["block_start"] <= payout_height and proposal["block_end"] >= payout_height + CYCLE_BLOCKS - 1
