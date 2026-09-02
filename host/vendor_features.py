#!/usr/bin/env python3
"""Vendor feature research helper for DirtyJTAG.

The JSON database intentionally stores clean-room feature ideas and source URLs.
It is not a compatibility claim and does not emulate proprietary vendor protocols.
"""
from __future__ import annotations
import argparse, csv, json, os, sys
from collections import Counter, defaultdict
from typing import Iterable, List, Dict, Any

DB_PATH = os.path.join(os.path.dirname(__file__), "vendor_feature_profiles.json")
REQUIRED = {"vendor", "product", "category", "features", "takeaway", "dirtyjtag_status", "priority", "source_url"}


def load_profiles(path: str = DB_PATH) -> List[Dict[str, Any]]:
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, list):
        raise ValueError("vendor feature database must be a JSON list")
    seen = set()
    for i, row in enumerate(data):
        missing = REQUIRED - set(row)
        if missing:
            raise ValueError(f"row {i} missing fields: {sorted(missing)}")
        if not isinstance(row["features"], list) or not row["features"]:
            raise ValueError(f"row {i} has no feature list")
        key = (row["vendor"].lower(), row["product"].lower())
        if key in seen:
            raise ValueError(f"duplicate vendor/product row: {row['vendor']} / {row['product']}")
        seen.add(key)
        if int(row["priority"]) < 1 or int(row["priority"]) > 10:
            raise ValueError(f"row {i} priority outside 1..10")
    return data


def normalized_text(row: Dict[str, Any]) -> str:
    return " ".join([
        row["vendor"], row["product"], row["category"], row["takeaway"],
        row["dirtyjtag_status"], " ".join(row["features"]), row["source_url"],
    ]).lower()


def filter_profiles(rows: Iterable[Dict[str, Any]], query: str = "", status: str | None = None,
                    min_priority: int = 1) -> List[Dict[str, Any]]:
    q = (query or "").lower().strip()
    out = []
    for row in rows:
        if status and row["dirtyjtag_status"] != status:
            continue
        if int(row["priority"]) < min_priority:
            continue
        if q and q not in normalized_text(row):
            continue
        out.append(row)
    return sorted(out, key=lambda r: (-int(r["priority"]), r["vendor"].lower(), r["product"].lower()))


def feature_counter(rows: Iterable[Dict[str, Any]]) -> Counter:
    c = Counter()
    for row in rows:
        for f in row["features"]:
            c[f.lower()] += 1
    return c


def category_summary(rows: Iterable[Dict[str, Any]]) -> Dict[str, int]:
    return dict(Counter(r["category"] for r in rows))


def status_summary(rows: Iterable[Dict[str, Any]]) -> Dict[str, int]:
    return dict(Counter(r["dirtyjtag_status"] for r in rows))


def roadmap(rows: Iterable[Dict[str, Any]], top: int = 20) -> List[Dict[str, Any]]:
    candidates = [r for r in rows if r["dirtyjtag_status"] in ("planned", "research", "implemented-partial")]
    return sorted(candidates, key=lambda r: (-int(r["priority"]), r["dirtyjtag_status"], r["vendor"].lower()))[:top]


def print_table(rows: List[Dict[str, Any]]) -> None:
    for r in rows:
        feats = "; ".join(r["features"][:4])
        print(f"{int(r['priority']):2d}  {r['dirtyjtag_status']:<19s}  {r['vendor']:<22s}  {r['product']:<32s}  {feats}")


def export_csv(rows: List[Dict[str, Any]], fp) -> None:
    w = csv.writer(fp)
    w.writerow(["priority", "status", "vendor", "product", "category", "features", "takeaway", "source_url"])
    for r in rows:
        w.writerow([r["priority"], r["dirtyjtag_status"], r["vendor"], r["product"],
                    r["category"], "; ".join(r["features"]), r["takeaway"], r["source_url"]])


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Query DirtyJTAG vendor feature deepsearch database")
    ap.add_argument("--db", default=DB_PATH)
    sp = ap.add_subparsers(dest="cmd", required=True)
    ls = sp.add_parser("list")
    ls.add_argument("--query", default="")
    ls.add_argument("--status")
    ls.add_argument("--min-priority", type=int, default=1)
    ls.add_argument("--limit", type=int, default=0)
    sh = sp.add_parser("show")
    sh.add_argument("vendor")
    summ = sp.add_parser("summary")
    rd = sp.add_parser("roadmap")
    rd.add_argument("--top", type=int, default=20)
    ex = sp.add_parser("export-csv")
    ex.add_argument("output")
    args = ap.parse_args(argv)
    rows = load_profiles(args.db)
    if args.cmd == "list":
        out = filter_profiles(rows, args.query, args.status, args.min_priority)
        if args.limit:
            out = out[:args.limit]
        print_table(out)
    elif args.cmd == "show":
        out = filter_profiles(rows, args.vendor)
        if not out:
            raise SystemExit(f"no vendor/product matches {args.vendor!r}")
        for r in out:
            print(f"{r['vendor']} - {r['product']}")
            print(f"  category: {r['category']}")
            print(f"  status:   {r['dirtyjtag_status']}  priority={r['priority']}")
            print(f"  features: {', '.join(r['features'])}")
            print(f"  takeaway: {r['takeaway']}")
            print(f"  source:   {r['source_url']}")
    elif args.cmd == "summary":
        print(f"vendors/products: {len(rows)}")
        print("status:", json.dumps(status_summary(rows), sort_keys=True))
        print("categories:", json.dumps(category_summary(rows), sort_keys=True))
        print("top feature terms:")
        for name, count in feature_counter(rows).most_common(20):
            print(f"  {count:2d}  {name}")
    elif args.cmd == "roadmap":
        print_table(roadmap(rows, args.top))
    elif args.cmd == "export-csv":
        with open(args.output, "w", newline="", encoding="utf-8") as f:
            export_csv(rows, f)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
