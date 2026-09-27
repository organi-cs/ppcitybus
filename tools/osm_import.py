#!/usr/bin/env python3
"""Bootstrap stops.csv and lines.csv from OpenStreetMap bus route relations.

Reads route=bus relations inside a bounding box through the Overpass API (or a
saved Overpass JSON response) and writes one lines.csv row per relation, using
its platform members in order. OSM has no timetable data, so every line is
written with approx=1 and empty headway/avg columns: fill those in by hand.

OSM data is (c) OpenStreetMap contributors, licensed under ODbL 1.0. Keep the
attribution in the data directory's SOURCES.md.

Usage:
  python tools/osm_import.py --lines 1A,2,3 --out data/sample
  python tools/osm_import.py --lines 1A,2,3 --input saved.json --out data/sample
"""

import argparse
import csv
import json
import re
import sys
import urllib.parse
import urllib.request
from pathlib import Path

OVERPASS_URL = "https://overpass-api.de/api/interpreter"
USER_AGENT = "ppbus-osm-import/0.1 (+https://github.com/organi-cs/ppcitybus)"
PHNOM_PENH_BBOX = "11.40,104.75,11.70,105.05"  # south,west,north,east

# "Stop 44 - Aeon Mall I - Bus Line 05" -> "Aeon Mall I". Stop numbers in OSM
# names are unreliable (see the note tags on Phnom Penh stops), so drop them.
STOP_LABEL = re.compile(r"^(?:Stop\s+\d+\s*-\s*)?(.*?)\s*-\s*(?:Bus\s+)?Line\s+\w+\b.*$")
# "Bus 2: Old Stadium Roundabout → Prek Samrong Bridge" -> "Old Stadium Roundabout → ..."
LINE_PREFIX = re.compile(r"^(?:Bus|Line|Linie)\s+\w+\s*[:/]\s*")


def build_query(bbox: str, refs: list[str]) -> str:
    ref_regex = "^(" + "|".join(re.escape(r) for r in refs) + ")$"
    return (
        "[out:json][timeout:120];"
        f'rel({bbox})["type"="route"]["route"="bus"]["ref"~"{ref_regex}"]->.r;'
        ".r out body;node(r.r)->.n;.n out body;"
    )


def fetch_overpass(query: str) -> dict:
    data = urllib.parse.urlencode({"data": query}).encode()
    request = urllib.request.Request(OVERPASS_URL, data=data, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=180) as response:
        return json.load(response)


def clean_stop_name(name: str) -> str:
    match = STOP_LABEL.match(name.strip())
    return (match.group(1) if match else name).strip()


def is_ascii(text: str) -> bool:
    return all(ord(c) < 128 for c in text)


def stop_names(tags: dict) -> tuple[str, str]:
    name = tags.get("name", "")
    name_en = tags.get("name:en") or (name if is_ascii(name) else "")
    name_km = tags.get("name:km") or ("" if is_ascii(name) else name)
    name_en = clean_stop_name(name_en)
    if name_en in ("", "."):
        name_en = ""
    return name_en, name_km


def ordered_platforms(relation: dict, nodes: dict) -> list[int]:
    members = [m for m in relation["members"] if m["type"] == "node" and m["ref"] in nodes]
    platforms = [m["ref"] for m in members if m["role"].startswith("platform")]
    if platforms:
        return platforms
    return [m["ref"] for m in members if m["role"].startswith("stop")]


def line_name(tags: dict) -> str:
    if tags.get("from") and tags.get("to"):
        return f"{tags['from']} → {tags['to']}"
    return LINE_PREFIX.sub("", tags.get("name", "")).strip()


def convert(osm: dict, refs: list[str]) -> tuple[list[dict], list[dict], list[str]]:
    nodes = {e["id"]: e for e in osm["elements"] if e["type"] == "node"}
    relations = sorted(
        (e for e in osm["elements"] if e["type"] == "relation" and e["tags"].get("ref") in refs),
        key=lambda r: (refs.index(r["tags"]["ref"]), r["id"]),
    )

    stops: dict[int, dict] = {}
    lines: list[dict] = []
    problems: list[str] = []
    direction_count: dict[str, int] = {}
    for rel in relations:
        ref = rel["tags"]["ref"]
        platforms = ordered_platforms(rel, nodes)
        if len(platforms) < 2:
            problems.append(f"relation {rel['id']} (line {ref}) has {len(platforms)} platform members; skipped")
            continue
        for node_id in platforms:
            if node_id in stops:
                continue
            node = nodes[node_id]
            name_en, name_km = stop_names(node.get("tags", {}))
            if not name_en:
                name_en = f"Unnamed stop {node_id}"
                problems.append(f"node {node_id} has no English name; called it '{name_en}'")
            stops[node_id] = {
                "id": f"N{node_id}",
                "name_en": name_en,
                "name_km": name_km,
                "lat": f"{node['lat']:.7f}",
                "lon": f"{node['lon']:.7f}",
                "station": "",
                "aliases": "",
                "osm_id": str(node_id),
            }
        direction = direction_count.get(ref, 0)
        direction_count[ref] = direction + 1
        lines.append({
            "line": ref,
            "direction": str(direction),
            "name": line_name(rel["tags"]),
            "headway_min": "",
            "avg_min_between_stops": "",
            "color": rel["tags"].get("colour", ""),
            "approx": "1",
            "stops": ";".join(f"N{n}" for n in platforms),
        })
    for ref in refs:
        if ref not in direction_count:
            problems.append(f"line {ref}: no usable relation found")
    return list(stops.values()), lines, problems


def write_csv(path: Path, fields: list[str], rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--lines", required=True, help="comma-separated line refs, e.g. 1A,2,3")
    parser.add_argument("--out", required=True, type=Path, help="directory for stops.csv and lines.csv")
    parser.add_argument("--bbox", default=PHNOM_PENH_BBOX, help="south,west,north,east")
    parser.add_argument("--input", type=Path, help="saved Overpass JSON instead of querying the API")
    parser.add_argument("--save", type=Path, help="also save the raw Overpass JSON here")
    parser.add_argument("--force", action="store_true", help="overwrite existing CSV files")
    args = parser.parse_args()

    refs = [r.strip() for r in args.lines.split(",") if r.strip()]
    stops_path, lines_path = args.out / "stops.csv", args.out / "lines.csv"
    if not args.force and (stops_path.exists() or lines_path.exists()):
        print(f"{args.out} already has stops.csv or lines.csv; use --force to overwrite", file=sys.stderr)
        return 1

    if args.input:
        osm = json.loads(args.input.read_text(encoding="utf-8"))
    else:
        osm = fetch_overpass(build_query(args.bbox, refs))
        if args.save:
            args.save.write_text(json.dumps(osm, ensure_ascii=False), encoding="utf-8")

    stops, lines, problems = convert(osm, refs)
    args.out.mkdir(parents=True, exist_ok=True)
    write_csv(stops_path, ["id", "name_en", "name_km", "lat", "lon", "station", "aliases", "osm_id"], stops)
    write_csv(lines_path, ["line", "direction", "name", "headway_min", "avg_min_between_stops", "color", "approx",
                           "stops"], lines)

    for problem in problems:
        print(f"warning: {problem}", file=sys.stderr)
    timestamp = osm.get("osm3s", {}).get("timestamp_osm_base", "unknown")
    print(f"wrote {len(stops)} stops and {len(lines)} line directions (OSM data as of {timestamp})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
