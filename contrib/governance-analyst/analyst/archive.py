"""Retain reviewed cycle packages locally, without claiming they were deployed."""
from .contract import review_documents, validate_report
from .core import ROOT, DataError, digest, read_json, write_json


def reviewed_packages():
    packages = []
    for path in sorted((ROOT / "data/reviewed").glob("cycle-*.json")):
        package = read_json(path)
        validate_report(package["report"])
        report = package["report"]
        expected = digest({"report": report, "documents": package["documents"]})
        if report["status"] != "reviewed" or report["cycle"] is None or package["reviews"]["package_sha256"] != expected:
            raise DataError("Archived review package integrity failed: " + str(path))
        for review in package["reviews"]["proposals"]:
            if review["decision"] != "approved" or review["reviewer"] != "Michael Korovkin" or review["package_sha256"] != expected:
                raise DataError("Archived approval is missing or inconsistent")
        if {r["proposal_id"] for r in package["reviews"]["proposals"]} != {p["id"] for p in report["proposals"]}:
            raise DataError("Archived review coverage failed")
        packages.append(package)
    return packages


def package_report(report, reviews):
    return {"report": report, "reviews": reviews,
            "documents": review_documents(report)}


def retain_reviewed(package):
    cycle = package["report"]["cycle"]["id"]
    path = ROOT / "data/reviewed" / ("cycle-%s.json" % cycle)
    if path.exists() and read_json(path) != package:
        old = read_json(path)
        write_json(ROOT / "data/reviewed/corrections" / ("cycle-%s-%s.json" % (cycle, digest(old)[:16])), old)
    write_json(path, package)
