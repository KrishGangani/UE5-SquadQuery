"""P07 SquadQuery - event-frame statistics of a benchmark batch (H6, H7, and the exclusive-stat nesting check).

Usage:
    python -I analyse_events.py BATCH            (BATCH = the folder-less batch name, e.g. 2026-11-21_085156_S3baseline)

For every measured run of the batch (priming runs skipped) it reads the CSV capture in Benchmarks/Raw and reports, per
configuration and agent count (median across runs, then [min-max]):
  * AI time per frame: mean, p99, p99.9 and max. Event-driven configurations work on few frames, so p99 can miss
    their event frames entirely; p99.9 and max don't.
  * AI time in the event window: the frame with an SQBench/Threat event and the WINDOW_FRAMES frames after it,
    mean per window frame and the largest single frame, against the frames outside any window.
  * The nesting check: on frames where EQS and our decision code both run, both columns are non-zero, which is only
    possible if the decision code's time is recorded in its own exclusive scope and not also inside EQS's.
Writes Benchmarks/Summaries/<BATCH>_events.md and prints it.
"""
from __future__ import annotations

import csv
import json
import os
import statistics
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS_DIR)
import csv_stats  # noqa: E402  (same folder)

PROJECT_DIR = os.path.dirname(TOOLS_DIR)
RAW_DIR = os.path.join(PROJECT_DIR, "Benchmarks", "Raw")
SUMMARY_DIR = os.path.join(PROJECT_DIR, "Benchmarks", "Summaries")
WINDOW_FRAMES = 30   # half a second at the fixed 60 Hz step: covers a query burst and its time slicing
EQS = "Exclusive/GameThread/EnvQueryManager"
DECIDE = "Exclusive/GameThread/SquadQueryDecide"


def run_stats(path: str) -> dict:
    header, data, _, _ = csv_stats.load_capture(path)
    ai = csv_stats.column_values(header, data, csv_stats.AI_COLUMN)
    eqs = csv_stats.column_values(header, data, EQS) if EQS in header else [0.0] * len(ai)
    decide = csv_stats.column_values(header, data, DECIDE) if DECIDE in header else [0.0] * len(ai)
    in_window = [False] * len(data)
    for index, row in enumerate(data):
        if any(event == "SQBench/Threat" for event in csv_stats.frame_events(header, row)):
            for k in range(index, min(index + WINDOW_FRAMES + 1, len(data))):
                in_window[k] = True
    window = [v for v, w in zip(ai, in_window) if w]
    outside = [v for v, w in zip(ai, in_window) if not w]
    both = sum(1 for e, d in zip(eqs, decide) if e > 0.0 and d > 0.0)
    return {
        "mean": statistics.mean(ai), "p99": csv_stats.percentile(sorted(ai), 99.0),
        "p999": csv_stats.percentile(sorted(ai), 99.9), "max": max(ai),
        "window_mean": statistics.mean(window) if window else 0.0, "window_max": max(window) if window else 0.0,
        "outside_mean": statistics.mean(outside) if outside else 0.0, "outside_max": max(outside) if outside else 0.0,
        "frames_over_1ms": sum(1 for v in ai if v > 1.0), "frames_eqs_and_decide": both,
    }


def cell(values: list[float], digits: int = 3) -> str:
    return f"{statistics.median(values):.{digits}f} [{min(values):.{digits}f}-{max(values):.{digits}f}]" if values else "n/a"


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print(__doc__)
        return 2
    batch = argv[0]
    manifest = json.load(open(os.path.join(RAW_DIR, batch + "_manifest.json"), encoding="utf-8"))
    groups: dict[tuple, list[dict]] = {}
    for run in manifest["runs"]:
        if run.get("priming") or "capture" not in run:
            continue
        stats = run_stats(os.path.join(RAW_DIR, run["capture"]))
        stats["latency"] = run.get("decisions", {}).get("query_latency_mean_s")
        groups.setdefault((run["config"], run["agents"]), []).append(stats)

    lines = [f"# Event frames of batch {batch}", "",
             f"AI = {csv_stats.AI_COLUMN.split('=', 1)[1]} per frame, ms. Event window = the frame with an `SQBench/Threat` "
             f"event and the {WINDOW_FRAMES} frames after it. Each cell: median across runs, then [min-max].", "",
             "| Config | N | Runs | AI mean | AI p99 | AI p99.9 | AI max | Window mean | Window max | Outside mean | Outside max | Frames > 1 ms | Frames with EQS and decide both > 0 | Query latency mean (s) |",
             "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for (config, agents), runs in sorted(groups.items(), key=lambda kv: (kv[0][1], kv[0][0])):
        def col(name: str) -> list[float]:
            return [r[name] for r in runs if r.get(name) is not None]
        lines.append(f"| {config} | {agents} | {len(runs)} | {cell(col('mean'))} | {cell(col('p99'))} | {cell(col('p999'))} | "
                     f"{cell(col('max'))} | {cell(col('window_mean'))} | {cell(col('window_max'))} | {cell(col('outside_mean'))} | "
                     f"{cell(col('outside_max'))} | {cell(col('frames_over_1ms'), 0)} | {cell(col('frames_eqs_and_decide'), 0)} | "
                     f"{cell(col('latency'), 3)} |")
    os.makedirs(SUMMARY_DIR, exist_ok=True)
    path = os.path.join(SUMMARY_DIR, batch + "_events.md")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    print("written:", path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
