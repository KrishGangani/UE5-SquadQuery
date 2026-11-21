"""P07 SquadQuery - statistics for Unreal CSV-profiler captures (adapted from UE5-UpdateBudget's Tools/csv_stats.py).

No tool shipped with UE 5.8 reports p95/p99, so this script does.

Usage:
    python csv_stats.py CAPTURE.csv [CAPTURE2.csv ...] [--columns A,B,NAME=C+D,...] [--skip N] [--json OUT.json]
    python csv_stats.py --selftest

Rules (documented so every number can be reproduced):
  * A finalised Unreal capture ends with two non-data rows: the full header and a "[key],value,..." metadata row.
    Both are dropped, and the END header is used for column lookup (the first header is only a prefix of it when
    columns appear mid-capture). If they are missing (crash or partial write) every row is data and the result is flagged.
  * The metadata row can be very long, so the csv field-size limit is raised.
  * The EVENTS column holds "Category/Name##timestamp" entries separated by ";".
  * Percentiles use linear interpolation between closest ranks (numpy's default "linear" method):
      position = p/100 * (n - 1);  value = lower + (upper - lower) * fraction.
  * A composite column "NAME=A+B[+C...]" is the per-frame sum of its parts. A missing or empty cell counts as 0, and
    a frame counts if at least one of the parts exists in the header. This is how "AI time" is computed: EQS's own
    exclusive stat, ours, and the engine's path-finding stat (AI_COLUMN; 01_Problem_Analysis.md section 6.3).
  * --skip N drops the first N data frames (for captures that start at boot; event-started captures need 0).
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import sys

# AI time per frame: EQS, our decision code, and the path queries decisions start. The engine's FindPath opens its own
# exclusive scope (RecastNavMesh.cpp:3839), which takes that time out of SquadQueryDecide, so it is added back here.
AI_COLUMN = ("AI=Exclusive/GameThread/EnvQueryManager+Exclusive/GameThread/SquadQueryDecide"
             "+Exclusive/GameThread/Pathfinding")

DEFAULT_COLUMNS = (
    "FrameTime",
    "GameThreadTime",
    "RenderThreadTime",
    "GPUTime",
    AI_COLUMN,
    "Exclusive/GameThread/EnvQueryManager",
    "Exclusive/GameThread/SquadQueryDecide",
    "Exclusive/GameThread/Pathfinding",
    "SQBench/InCover",
)

csv.field_size_limit(1 << 30)


def percentile(sorted_values: list[float], p: float) -> float:
    """Linear-interpolation percentile of an already sorted, non-empty list. p in [0, 100]."""
    if not sorted_values:
        raise ValueError("percentile of an empty list")
    if len(sorted_values) == 1:
        return sorted_values[0]
    position = (p / 100.0) * (len(sorted_values) - 1)
    lower = math.floor(position)
    upper = min(lower + 1, len(sorted_values) - 1)
    fraction = position - lower
    return sorted_values[lower] + (sorted_values[upper] - sorted_values[lower]) * fraction


def summarize(values: list[float]) -> dict:
    """Count, mean, min, median, p95, p99 and max of a list of numbers."""
    ordered = sorted(values)
    return {
        "n": len(ordered),
        "mean": sum(ordered) / len(ordered),
        "min": ordered[0],
        "median": percentile(ordered, 50.0),
        "p95": percentile(ordered, 95.0),
        "p99": percentile(ordered, 99.0),
        "max": ordered[-1],
    }


def load_capture(path: str) -> tuple[list[str], list[list[str]], dict, bool]:
    """Return (header, data rows, metadata dict, finalised) of an Unreal CSV capture."""
    with open(path, newline="", encoding="utf-8", errors="replace") as handle:
        rows = list(csv.reader(handle))
    if len(rows) < 2:
        raise ValueError(f"{path}: too few rows to be an Unreal capture")
    first_header = rows[0]
    finalised = (len(rows) >= 3 and bool(rows[-1]) and rows[-1][0].strip().startswith("[")
                 and rows[-2][:len(first_header)] == first_header)
    if not finalised:
        return first_header, rows[1:], {}, False

    header, data, meta_row = rows[-2], rows[1:-2], rows[-1]
    metadata = {}
    for index in range(0, len(meta_row) - 1, 2):
        key = meta_row[index].strip().strip("[]").lower()
        if key:
            metadata[key] = meta_row[index + 1].strip()
    return header, data, metadata, True


def frame_events(header: list[str], row: list[str]) -> list[str]:
    """Event names ("Category/Name", timestamps removed) recorded on one frame."""
    lowered = [h.lower() for h in header]
    if "events" not in lowered:
        return []
    index = lowered.index("events")
    if index >= len(row) or not row[index].strip():
        return []
    return [entry.split("##")[0].strip() for entry in row[index].split(";") if entry.strip()]


def column_values(header: list[str], rows: list[list[str]], column: str) -> list[float]:
    """Numeric values of one column, or of a composite "NAME=A+B" column. Raises ValueError if absent."""
    if "=" in column:
        parts = column.split("=", 1)[1].split("+")
        indices = [header.index(part) for part in parts if part in header]
        if not indices:
            raise ValueError(f"none of the parts of {column} is in the capture")
        values = []
        for row in rows:
            cells = [row[i] for i in indices if i < len(row) and row[i].strip() != ""]
            values.append(sum(float(cell) for cell in cells))
        return values
    index = header.index(column)
    return [float(row[index]) for row in rows if index < len(row) and row[index].strip() != ""]


def column_label(column: str) -> str:
    return column.split("=", 1)[0] if "=" in column else column


def has_column(header: list[str], column: str) -> bool:
    if "=" in column:
        return any(part in header for part in column.split("=", 1)[1].split("+"))
    return column in header


def summarize_rows(header: list[str], rows: list[list[str]], columns: tuple[str, ...]) -> dict:
    result = {}
    for column in columns:
        if has_column(header, column):
            values = column_values(header, rows, column)
            if values:
                result[column_label(column)] = summarize(values)
    return result


def analyse(path: str, columns: tuple[str, ...], skip: int = 0) -> dict:
    header, data, metadata, finalised = load_capture(path)
    data = data[skip:]
    event_counts: dict[str, int] = {}
    for row in data:
        for event in frame_events(header, row):
            key = event.split("/")[0] if event.startswith("PSO/") else event
            event_counts[key] = event_counts.get(key, 0) + 1
    return {"file": os.path.basename(path), "frames": len(data), "finalised": finalised,
            "metadata": metadata, "events": event_counts, "columns": summarize_rows(header, data, columns)}


def selftest() -> None:
    """Checks the statistics and parsing against values worked out by hand."""
    values = [float(v) for v in range(1, 11)]                   # 1..10
    assert percentile(values, 50) == 5.5                          # position 4.5 -> 5 + 0.5*(6-5)
    assert abs(percentile(values, 95) - 9.55) < 1e-12             # position 8.55 -> 9 + 0.55*1
    assert abs(percentile(values, 99) - 9.91) < 1e-12             # position 8.91 -> 9 + 0.91*1
    assert percentile([7.0], 99) == 7.0
    s = summarize([4.0, 1.0, 3.0, 2.0])
    assert s["n"] == 4 and s["min"] == 1.0 and s["max"] == 4.0 and s["mean"] == 2.5 and s["median"] == 2.5
    header = ["EVENTS", "FrameTime", "A", "B"]
    assert frame_events(header, ["SQBench/Threat##1.5;PSO/Encountered new graphics PSO##1.6", "3", "", ""]) == \
        ["SQBench/Threat", "PSO/Encountered new graphics PSO"]
    assert frame_events(header, ["", "3", "", ""]) == []
    # Composite column: per-frame sum, empty cell = 0; a part missing from the header is ignored.
    rows = [["", "1", "0.5", "0.25"], ["", "1", "", "1.0"], ["", "1", "2.0", ""]]
    assert column_values(header, rows, "AI=A+B") == [0.75, 1.0, 2.0]
    assert column_values(header, rows, "AI=A+Missing") == [0.5, 0.0, 2.0]
    assert summarize_rows(header, rows, ("AI=A+B",))["AI"]["mean"] == (0.75 + 1.0 + 2.0) / 3
    print("selftest: OK (percentiles, summary, event parsing and composite columns match hand-worked values)")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("captures", nargs="*")
    parser.add_argument("--columns", default=",".join(DEFAULT_COLUMNS))
    parser.add_argument("--skip", type=int, default=0)
    parser.add_argument("--json")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)

    if args.selftest:
        selftest()
        return 0
    if not args.captures:
        parser.error("give at least one capture, or --selftest")

    columns = tuple(c.strip() for c in args.columns.split(",") if c.strip())
    results = [analyse(path, columns, args.skip) for path in args.captures]
    for result in results:
        flag = "" if result["finalised"] else "  [NOT FINALISED: partial capture, treat with care]"
        print(f"{result['file']}  ({result['frames']} frames, tag={result['metadata'].get('sqruntag', '?')}, events={result['events']}){flag}")
        for column, s in result["columns"].items():
            print(f"  {column:<40} mean {s['mean']:9.3f}  median {s['median']:9.3f}  p95 {s['p95']:9.3f}  p99 {s['p99']:9.3f}  max {s['max']:9.3f}")
    if args.json:
        with open(args.json, "w", encoding="utf-8") as handle:
            json.dump(results, handle, indent=2)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
