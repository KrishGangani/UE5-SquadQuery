"""P07 SquadQuery - run a benchmark batch on the packaged build and collect every artefact.
(Adapted from UE5-UpdateBudget's Tools/run_bench.py: same protocol, this project's configurations and metrics.)

Usage (from anywhere):
    python run_bench.py --configs B0,B1,B2,SQ --agents 8,32,128 [--runs N] [--capture 60] [--warmup 10]
                        [--interval 6] [--seed 1] [--label NAME] [--cooldown 5] [--no-prime] [--dry-run]

What one batch does:
  1. Pins this process to the performance cores (hybrid CPU), so every game it starts inherits the mask from its
     first instruction, then records the environment once, including the mask Windows actually applied.
  2. Runs one PRIMING run first (discarded), so file, driver, shader and PSO caches are warm for every measured run.
  3. Runs every (config x agents) combination --runs times, INTERLEAVED: the config order rotates every round, so each
     config sits in each order position equally often and thermal drift spreads over all configs.
  4. Before each run, deletes the build's GameUserSettings.ini (it can override VSync / frame-rate caps).
  5. Launches the real game executable directly (not the bootstrap and not "cmd /c start", which always exits 0),
     waits with a timeout and kills the process tree in a finally block.
  6. After each run, moves the CSV capture, the run reports (_agents.csv, _events.csv) and the run's own log into
     Benchmarks/Raw as <batch>_<Config>_N<agents>_r<run>.<ext>, where <batch> = YYYY-MM-DD_HHMMSS[_label].
     Existing files are never overwritten. The manifest is rewritten after every run, so a crash loses nothing.
  7. Writes Benchmarks/Summaries/<batch>_summary.md: per combination, the median across runs of each run's statistic,
     then [min-max across runs], for AI time, frame time and the decision-quality metrics.

Every run uses: fixed step (-deterministic -FPS=60), real GPU at 1280x720 windowed, no VSync, no sound,
-traceautostart=0, and a capture window started/stopped by the game's BenchStart/BenchEnd events.
"""
from __future__ import annotations

import argparse
import csv
import ctypes
import datetime as dt
import glob
import json
import math
import os
import shutil
import statistics
import struct
import subprocess
import sys
import time

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS_DIR)
import csv_stats  # noqa: E402  (same folder)

PROJECT_DIR = os.path.dirname(TOOLS_DIR)
BUILD_DIR = os.path.join(PROJECT_DIR, "Build", "Windows")
# The real game executable, launched directly. (The packaged bootstrap Build\Windows\SquadQuery.exe only starts this
# one; launching it directly makes the game a direct child, so its exit code and its kill are reliable.)
EXE = os.path.join(BUILD_DIR, "SquadQuery", "Binaries", "Win64", "SquadQuery.exe")
SAVED = os.path.join(BUILD_DIR, "SquadQuery", "Saved")
RAW_DIR = os.path.join(PROJECT_DIR, "Benchmarks", "Raw")
SUMMARY_DIR = os.path.join(PROJECT_DIR, "Benchmarks", "Summaries")
ENGINE_BUILD_VERSION = r"C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version"
MIN_RUNS = 4                    # fewest measured runs per combination (Benchmarks/README.md, measurement rules)
MIN_WALK_CM = 100.0             # the walk ratio ignores agents that were already (almost) at their slot
IN_COVER_RADIUS_CM = 75.0       # USQBenchmarkSubsystem::InCoverRadiusCm: farther from its slot at the end = stuck
STARTUP_MARGIN_SECONDS = 240.0  # engine start, navmesh build, spawning and a first-run PSO wait at shutdown (09, Phase 4)

COMMON_ARGS = ("-windowed -ResX=1280 -ResY=720 -nosound -unattended -traceautostart=0 "
               "-deterministic -FPS=60 -novsync -SQExit -csvStartOnEvent=BenchStart -csvStopOnEvent=BenchEnd")

# Extra command-line arguments per configuration (02_System_Architecture.md section 9). Each entry is added in the
# step whose code implements it: Scripted (S2, a metrics test oracle, not a benchmark configuration), B0-B2 (S3), SQ (S5).
CONFIGS = {
    "Scripted": "-SQMode=Scripted",
}

AI = csv_stats.AI_COLUMN
SUMMARY_COLUMNS = csv_stats.DEFAULT_COLUMNS   # one list of columns for the stats script and the manifest


def powershell(command: str) -> str:
    result = subprocess.run(["powershell", "-NoProfile", "-Command", command], capture_output=True, text=True)
    return result.stdout.strip()


KERNEL32 = None
if sys.platform == "win32":
    KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)
    KERNEL32.GetCurrentProcess.restype = ctypes.c_void_p
    KERNEL32.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    KERNEL32.GetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]


def performance_core_mask() -> int | None:
    """Affinity mask of the performance cores on a hybrid CPU; None on a non-hybrid CPU or off Windows.

    Why: on this i7-14650HX (8 P-cores + 8 E-cores) UE5-UpdateBudget's baseline batch showed runs at two stable levels
    about 1.6x apart, with Windows placing the game thread on an E-core. Pinning to the P-cores gives every run what a
    foreground game normally gets. Source of truth: GetLogicalProcessorInformationEx(RelationProcessorCore) reports an
    EfficiencyClass per physical core; the P-cores have the higher class. Only processor group 0 exists on a machine
    with fewer than 64 logical CPUs. (Copied from UE5-UpdateBudget.)
    """
    if KERNEL32 is None:
        return None
    length = ctypes.c_ulong(0)
    KERNEL32.GetLogicalProcessorInformationEx(0, None, ctypes.byref(length))
    buffer = ctypes.create_string_buffer(length.value)
    if not length.value or not KERNEL32.GetLogicalProcessorInformationEx(0, buffer, ctypes.byref(length)):
        return None
    raw, offset, masks = buffer.raw, 0, {}          # efficiency class -> combined mask of its cores
    while offset < length.value:
        relationship, size = struct.unpack_from("<II", raw, offset)
        if relationship == 0 and size >= 48:                     # RelationProcessorCore
            efficiency_class = raw[offset + 9]                   # PROCESSOR_RELATIONSHIP.EfficiencyClass
            mask, group = struct.unpack_from("<QH", raw, offset + 32)   # GroupMask[0]: KAFFINITY Mask, WORD Group
            if group == 0:
                masks[efficiency_class] = masks.get(efficiency_class, 0) | mask
        offset += size
    return masks[max(masks)] if len(masks) > 1 else None


PERFORMANCE_CORE_MASK = performance_core_mask()


def pin_self(mask: int) -> bool:
    """Restricts this batch process to the CPUs in mask. Every process it starts afterwards inherits the mask from its
    first instruction, so each game runs pinned for its whole life, engine start-up included. (Copied from UE5-UpdateBudget.)"""
    return bool(KERNEL32.SetProcessAffinityMask(KERNEL32.GetCurrentProcess(), mask))


def applied_mask() -> str:
    """The affinity this process actually has (read back from Windows, not the one requested). (Copied from UE5-UpdateBudget.)"""
    if KERNEL32 is None:
        return "not Windows"
    process_mask, system_mask = ctypes.c_size_t(0), ctypes.c_size_t(0)
    KERNEL32.GetProcessAffinityMask(KERNEL32.GetCurrentProcess(), ctypes.byref(process_mask), ctypes.byref(system_mask))
    if process_mask.value == system_mask.value:
        return f"0x{process_mask.value:X} (all logical processors)"
    return f"0x{process_mask.value:X} (performance cores only; hybrid CPU)"


def environment_record() -> dict:
    """Facts that must accompany every result (Benchmarks/README.md, environment record)."""
    with open(ENGINE_BUILD_VERSION, encoding="utf-8") as handle:
        build = json.load(handle)
    return {
        "date": dt.datetime.now().isoformat(timespec="seconds"),
        "engine": f"{build['MajorVersion']}.{build['MinorVersion']}.{build['PatchVersion']} (CL {build['Changelist']})",
        "build_config": "Development (packaged)",
        "compiler": "MSVC 14.44.35229, Windows SDK 10.0.22621.0",
        "cpu": powershell("(Get-CimInstance Win32_Processor).Name"),
        "gpus": powershell("(Get-CimInstance Win32_VideoController | ForEach-Object { $_.Name + ' (driver ' + $_.DriverVersion + ')' }) -join '; '"),
        "ram_gb": powershell("[math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1GB,1)"),
        "os": powershell("$o=Get-CimInstance Win32_OperatingSystem; $o.Caption + ' build ' + $o.BuildNumber"),
        "power_plan": powershell("powercfg /getactivescheme"),
        "on_ac_power": powershell("(Get-CimInstance Win32_Battery | Select-Object -First 1).BatteryStatus") == "2",
        "resolution": "1280x720 windowed, default scalability",
        "eqs": "MaxAllowedTestingTime 0.01 s, breadth first (engine default, restated in DefaultGame.ini); EQS debugger on (Development)",
        "common_args": COMMON_ARGS,
        "cpu_affinity": applied_mask(),
    }


def unique_path(path: str) -> str:
    """Raw data is never overwritten (Benchmarks/README.md)."""
    if os.path.exists(path):
        raise FileExistsError(f"refusing to overwrite raw data: {path}")
    return path


def launch(name: str, config: str, agents: int, args) -> tuple[str, int | None, float, float]:
    """Starts one run and waits for it. Returns (command, exit code or None on timeout, start time, wall seconds)."""
    sq_args = (f"-SQAgents={agents} -SQSeed={args.seed} -SQWarmup={args.warmup} -SQCapture={args.capture} "
               f"-SQEventInterval={args.interval} -SQRunTag={name}")
    command = f'"{EXE}" {COMMON_ARGS} {sq_args} {CONFIGS[config]}'.strip()
    if args.dry_run:
        print("DRY:", command)
        return command, 0, time.time(), 0.0

    gus = os.path.join(SAVED, "Config", "Windows", "GameUserSettings.ini")
    if os.path.exists(gus):
        os.remove(gus)
    started = time.time()
    process = subprocess.Popen(command)   # inherits this process's CPU affinity (main: pin_self)
    try:
        exit_code = process.wait(timeout=args.warmup + args.capture + STARTUP_MARGIN_SECONDS)
    except subprocess.TimeoutExpired:
        exit_code = None
    finally:
        if process.poll() is None:   # timeout, Ctrl+C or any other error: never leave a game running into the next run
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)], capture_output=True)
    return command, exit_code, started, time.time() - started


def read_report(path: str) -> tuple[dict, list[dict]]:
    """A run report: the '# key=value ...' header line, then CSV rows."""
    with open(path, encoding="utf-8") as handle:
        header_line = handle.readline().lstrip("#").split()
        header = dict(item.split("=", 1) for item in header_line if "=" in item)
        rows = list(csv.DictReader(handle))
    return header, rows


def decision_metrics(agents_path: str, events_path: str) -> dict:
    """Decision quality of one run (01_Problem_Analysis.md section 6.3), from the two run reports."""
    header, agents = read_report(agents_path)
    _, events = read_report(events_path)
    captured_events = int(header.get("captured_events", 0))
    capture_minutes = float(header.get("capture", 60.0)) / 60.0
    seconds = [float(r["seconds_to_cover"]) for r in events]
    reached = sorted(s for s in seconds if s >= 0.0)
    # An agent whose slot stays valid cover for the new threat is in cover on the event frame (0 s). Those zeros are
    # most of the samples when the threat moves only a little, so time to cover is also given over the agents that
    # had to move: the ones the event exposed.
    moved = sorted(s for s in seconds if s > 0.0)
    switches = sum(int(r["switches"] or 0) for r in agents)
    agent_frames = int(header.get("agent_frames", 0))
    result = {
        "captured_events": captured_events,
        "agents": len(agents),
        "conflict_onsets_per_min": int(header.get("conflict_onsets", 0)) / capture_minutes,
        "conflict_agent_frames": int(header.get("conflict_agent_frames", 0)),
        "in_cover_share": int(header.get("in_cover_agent_frames", 0)) / agent_frames if agent_frames else 0.0,
        "switches_per_agent_per_event": switches / (len(agents) * captured_events) if agents and captured_events else 0.0,
        "oscillations": sum(int(r["oscillations"] or 0) for r in agents),
        "move_requests": sum(int(r["move_requests"] or 0) for r in agents),
        "move_failures": sum(int(r.get("move_failures") or 0) for r in agents),
        "in_cover_by_next_event": len(reached) / len(seconds) if seconds else 0.0,
        "already_in_cover": sum(1 for s in seconds if s == 0.0) / len(seconds) if seconds else 0.0,
        "move_retries": sum(int(r.get("move_retries") or 0) for r in agents),
        "stuck_at_end": sum(1 for r in agents if float(r.get("final_slot_distance_cm") or -1.0) > IN_COVER_RADIUS_CM),
    }
    if reached:
        result["time_to_cover_median"] = csv_stats.percentile(reached, 50.0)
        result["time_to_cover_p95"] = csv_stats.percentile(reached, 95.0)
    if moved:
        result["time_to_cover_moved_median"] = csv_stats.percentile(moved, 50.0)
        result["time_to_cover_moved_p95"] = csv_stats.percentile(moved, 95.0)
    # Time to cover over the fastest possible time (straight-line distance / walking speed), for agents that had at
    # least 1 m to go: 1.0 = no decision delay and no detour; above it, decision latency plus the path around blocks.
    speed = float(header.get("walkspeed", 0.0))
    ratios = sorted(float(r["seconds_to_cover"]) / (float(r["distance_cm"]) / speed)
                    for r in events
                    if speed > 0.0 and float(r.get("distance_cm", -1.0)) >= MIN_WALK_CM and float(r["seconds_to_cover"]) >= 0.0)
    if ratios:
        result["walk_ratio_median"] = csv_stats.percentile(ratios, 50.0)
        result["walk_ratio_p95"] = csv_stats.percentile(ratios, 95.0)
    return result


def collect(name: str, started: float) -> dict:
    """Moves this run's artefacts into Benchmarks/Raw and returns what was found."""
    record: dict = {}
    os.makedirs(RAW_DIR, exist_ok=True)

    csv_files = [p for p in glob.glob(os.path.join(SAVED, "Profiling", "CSV", "*.csv")) if os.path.getmtime(p) >= started]
    if csv_files:
        destination = unique_path(os.path.join(RAW_DIR, name + ".csv"))
        shutil.move(max(csv_files, key=os.path.getmtime), destination)
        record["capture"] = os.path.basename(destination)
        try:
            analysis = csv_stats.analyse(destination, SUMMARY_COLUMNS, 0)
            record["stats"] = analysis["columns"]
            record["finalised"] = analysis["finalised"]
            record["events"] = analysis["events"]
            record["frames"] = analysis["frames"]
        except (ValueError, IndexError) as error:
            record["analysis_error"] = str(error)

    reports = {}
    for suffix in ("_agents.csv", "_events.csv"):
        report = os.path.join(SAVED, "SQRuns", name + suffix)
        if os.path.exists(report):
            destination = unique_path(os.path.join(RAW_DIR, name + suffix))
            shutil.move(report, destination)
            reports[suffix] = destination
            record["report" + suffix.split(".")[0]] = os.path.basename(destination)
    if len(reports) == 2:
        try:
            record["decisions"] = decision_metrics(reports["_agents.csv"], reports["_events.csv"])
        except (ValueError, KeyError, ZeroDivisionError) as error:
            record["report_error"] = str(error)

    log = os.path.join(SAVED, "Logs", "SquadQuery.log")
    if os.path.exists(log) and os.path.getmtime(log) >= started:   # only this run's log, never a stale one
        with open(log, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        record["log_errors"] = text.count("Error:")
        record["log_warnings"] = text.count("Warning:")
        shutil.copy(log, unique_path(os.path.join(RAW_DIR, name + ".log")))
    else:
        record["log_missing"] = True
    return record


def is_failure(record: dict) -> bool:
    return (record.get("exit_code") != 0 or "stats" not in record or "decisions" not in record or record.get("log_errors")
            or record.get("log_missing") or record.get("finalised") is False or "analysis_error" in record
            or "report_error" in record)


def write_manifest(batch: str, environment: dict, records: list[dict]) -> None:
    os.makedirs(RAW_DIR, exist_ok=True)
    with open(os.path.join(RAW_DIR, batch + "_manifest.json"), "w", encoding="utf-8") as handle:
        json.dump({"environment": environment, "runs": records}, handle, indent=2)


def cell(values: list[float], digits: int = 3) -> str:
    """Median across runs, then [min-max across runs]."""
    if not values:
        return "n/a"
    return f"{statistics.median(values):.{digits}f} [{min(values):.{digits}f}-{max(values):.{digits}f}]"


def write_summary(batch: str, environment: dict, records: list[dict]) -> str:
    os.makedirs(SUMMARY_DIR, exist_ok=True)
    groups: dict[tuple, list[dict]] = {}
    for record in records:
        if "stats" in record and not record.get("priming"):
            groups.setdefault((record["config"], record["agents"]), []).append(record)

    lines = [f"# Benchmark batch {batch}", "", "## Environment", "", "```json", json.dumps(environment, indent=2), "```", "",
             "## Cost (the whole capture)", "",
             "Each cell: median across runs of each run's statistic, then [min-max across runs]. AI = the per-frame sum of "
             + " + ".join(part.rsplit("/", 1)[-1] for part in AI.split("=", 1)[1].split("+"))
             + " exclusive time (01_Problem_Analysis.md section 6.3).", "",
             "| Config | N | Runs | AI mean (ms) | AI p95 (ms) | AI p99 (ms) | GameThreadTime median (ms) | FrameTime median / p99 (ms) | PSO events | Log errors |",
             "|---|---|---|---|---|---|---|---|---|---|"]
    order = sorted(groups.items(), key=lambda kv: (kv[0][1], kv[0][0]))
    for (config, agents), group in order:
        def stat(column: str, name: str) -> list[float]:
            return [g["stats"][column][name] for g in group if column in g["stats"]]
        lines.append(f"| {config} | {agents} | {len(group)} | {cell(stat('AI', 'mean'))} | {cell(stat('AI', 'p95'))} | "
                     f"{cell(stat('AI', 'p99'))} | {cell(stat('GameThreadTime', 'median'))} | "
                     f"{cell(stat('FrameTime', 'median'))} / {cell(stat('FrameTime', 'p99'))} | "
                     f"{sum(g.get('events', {}).get('PSO', 0) for g in group)} | {sum(g.get('log_errors', 0) for g in group)} |")

    lines += ["", "## Decision quality", "",
              "| Config | N | Conflict onsets / min | Switches / agent / event | Oscillations | Already in cover | Time to cover, agents that moved: median / p95 (s) | Walk ratio median / p95 | In cover by next event | In-cover share | Stuck at end |",
              "|---|---|---|---|---|---|---|---|---|---|---|"]
    for (config, agents), group in order:
        decisions = [g["decisions"] for g in group if "decisions" in g]
        def metric(name: str) -> list[float]:
            return [d[name] for d in decisions if name in d]
        lines.append(f"| {config} | {agents} | {cell(metric('conflict_onsets_per_min'), 2)} | {cell(metric('switches_per_agent_per_event'), 2)} | "
                     f"{cell(metric('oscillations'), 0)} | {cell(metric('already_in_cover'), 3)} | "
                     f"{cell(metric('time_to_cover_moved_median'), 2)} / {cell(metric('time_to_cover_moved_p95'), 2)} | "
                     f"{cell(metric('walk_ratio_median'), 2)} / {cell(metric('walk_ratio_p95'), 2)} | "
                     f"{cell(metric('in_cover_by_next_event'), 3)} | {cell(metric('in_cover_share'), 3)} | {cell(metric('stuck_at_end'), 0)} |")
    lines += ["", f"Raw files: `Benchmarks/Raw/{batch}_*` (CSV, run reports and log per run) and `{batch}_manifest.json`."]
    path = os.path.join(SUMMARY_DIR, batch + "_summary.md")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    return path


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--configs", default="Scripted")
    parser.add_argument("--agents", default="32")
    parser.add_argument("--runs", type=int, default=0,
                        help=f"runs per combination; 0 (default) = the smallest multiple of the number of configs that is "
                             f"at least {MIN_RUNS}, so the rotation puts every config in every order position equally often")
    parser.add_argument("--warmup", type=float, default=10.0)
    parser.add_argument("--capture", type=float, default=60.0)
    parser.add_argument("--interval", type=float, default=6.0)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--cooldown", type=float, default=5.0)
    parser.add_argument("--label", default="")
    parser.add_argument("--no-prime", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    # The label becomes part of -SQRunTag=, which the game reads up to the first space (FParse::Value).
    if not all(ch.isalnum() or ch in "_-" for ch in args.label):
        parser.error(f"--label may contain only letters, digits, '_' and '-': {args.label!r}")
    configs = [c.strip() for c in args.configs.split(",") if c.strip()]
    unknown = [c for c in configs if c not in CONFIGS]
    if unknown:
        parser.error(f"unknown config(s) {unknown}; available: {sorted(CONFIGS)}")
    agent_counts = [int(a) for a in args.agents.split(",") if a.strip()]
    if args.runs <= 0:
        args.runs = math.ceil(MIN_RUNS / len(configs)) * len(configs)
    elif args.runs % len(configs):
        print(f"WARNING: --runs {args.runs} is not a multiple of {len(configs)} configs; the rotation is unbalanced")
    if not os.path.exists(EXE) and not args.dry_run:
        parser.error(f"packaged build not found: {EXE} (run Tools\\package.bat first)")

    # Keep the display and the system awake while the batch runs (an occluded or locked desktop can throttle a windowed
    # game). Applies only while this process lives (SetThreadExecutionState: ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED).
    if not args.dry_run and KERNEL32 is not None:
        KERNEL32.SetThreadExecutionState(0x80000000 | 0x00000001 | 0x00000002)
        # Pin once, before anything is recorded or started: every game started below inherits the mask.
        if PERFORMANCE_CORE_MASK and not pin_self(PERFORMANCE_CORE_MASK):
            parser.error(f"could not pin the batch to the performance cores (0x{PERFORMANCE_CORE_MASK:X}); refusing to "
                         "measure on a hybrid CPU without it")

    batch = dt.datetime.now().strftime("%Y-%m-%d_%H%M%S") + (f"_{args.label}" if args.label else "")
    environment = environment_record() if not args.dry_run else {}
    records: list[dict] = []

    def run(config: str, agents: int, run_index: int, priming: bool = False) -> None:
        name = f"{batch}_{config}_N{agents}_r{run_index}" + ("_prime" if priming else "")
        command, exit_code, started, wall = launch(name, config, agents, args)
        record = {"name": name, "config": config, "agents": agents, "run": run_index, "priming": priming,
                  "exit_code": exit_code, "wall_seconds": round(wall, 1), "command": command}
        if not args.dry_run:
            record.update(collect(name, started))
            records.append(record)
            write_manifest(batch, environment, records)
            ai = record.get("stats", {}).get("AI", {})
            decisions = record.get("decisions", {})
            print(f"  {name}: exit {exit_code}, {wall:.0f} s, AI mean {ai.get('mean', float('nan')):.3f} ms "
                  f"p99 {ai.get('p99', float('nan')):.3f} ms, conflicts/min {decisions.get('conflict_onsets_per_min', float('nan')):.2f}, "
                  f"TTC (moved) median {decisions.get('time_to_cover_moved_median', float('nan')):.2f} s, "
                  f"stuck {decisions.get('stuck_at_end', 'n/a')}, log errors {record.get('log_errors')}"
                  + ("  [PRIMING, discarded]" if priming else ""))
            time.sleep(args.cooldown)

    if not args.no_prime:
        run(configs[0], min(agent_counts), 0, priming=True)

    for run_index in range(1, args.runs + 1):
        rotation = (run_index - 1) % len(configs)
        round_configs = configs[rotation:] + configs[:rotation]       # interleave: rotate the order every round
        for agents in agent_counts:
            for config in round_configs:
                run(config, agents, run_index)

    if args.dry_run:
        return 0
    print("summary:", write_summary(batch, environment, records))
    failures = [r["name"] for r in records if not r.get("priming") and is_failure(r)]
    if failures:
        print("ATTENTION - runs with a non-zero exit, timeout, no/partial capture, missing report or log errors:", failures)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
