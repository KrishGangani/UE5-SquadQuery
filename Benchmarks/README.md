# P07 · Benchmarks

Every number in this project's write-up must trace back to a file in this folder. The method is in [../Explanation/01_Problem_Analysis.md §7](../Explanation/01_Problem_Analysis.md). The results, batch by batch, are in [../Explanation/09_Testing_And_Benchmarks.md](../Explanation/09_Testing_And_Benchmarks.md).

## Measurement rules

1. **Baselines first.** B0, B1 and B2 are captured in step S3, before any SquadQuery code exists, on the same arena, build and machine as every later run.
2. **Packaged Development build only** (never PIE, never the editor). The launcher engine can't build the Test configuration.
3. **Fixed step** (`-deterministic -FPS=60`), real GPU, windowed 1280×720, no VSync, no sound, `-traceautostart=0`. The capture window is opened and closed by the game itself (`BenchStart` / `BenchEnd` CSV events, `-ExitAfterCsvProfiling`), never by a timer. `BenchStart` fires on a threat event, so the 60 s capture holds exactly 10 complete event windows.
4. **Pinned to the performance cores.** The run script pins **itself** once with `pin_self(mask)` (mask from `performance_core_mask()`), before it records the environment or starts anything, so every game it launches inherits the mask from its first instruction. It launches the real game executable `Build\Windows\SquadQuery\Binaries\Win64\SquadQuery.exe` directly (`subprocess.Popen`), waits with a timeout and kills the process tree in a `finally` block. The three functions are copied from UE5-UpdateBudget's `Tools\run_bench.py`. `cmd /c start /affinity` is not used: it always returns exit code 0 and would hide a crash. The mask Windows actually applied (`applied_mask()`) is written into the environment record.
5. **One discarded priming run per batch**, then **runs per combination = the number of configurations in the batch**, interleaved, with the order rotated every round, so every configuration sits in every order position exactly once.
6. **Statistics:** per run, mean, median, p95 and p99 (linear-interpolation percentiles, `Tools\csv_stats.py`, from S2); across runs, the median with the [min–max]. An improvement is claimed only when the ranges don't overlap. FPS is never used as evidence. Decision metrics are reported as distributions over runs, because EQS's time slice is wall-clock and decisions can differ between runs.
7. **The environment is recorded with every batch** (below), automatically, by `Tools\run_bench.py` (from S2).

## Folders

| Folder | What goes in it | Rule |
|---|---|---|
| `Raw\` | CSV profiler captures (`.csv`), per-agent and per-event run reports (`_agents.csv`, `_events.csv`), game logs (`.log`), Unreal Insights traces (`.utrace`), one `_manifest.json` per batch | **Never edited.** Anything re-processed goes to `Summaries\`. Only the manifests are in git (exact command lines, environment, affinity masks, per-run statistics). The captures are too large for git, so every batch cited in the docs is attached as `<batch>_raw.zip` to this repository's **Releases** page |
| `Summaries\` | Computed statistics, SC1–SC7 pass/fail tables, charts | Each summary names the batch it was computed from |

## File naming

`<batch>_<Config>_N<agents>_r<run>.<ext>`, with `<batch>` = `YYYY-MM-DD_HHMMSS[_label]`, so two batches can never collide (`run_bench.py` refuses to overwrite).

- `<Config>`: `B0` per-agent EQS, polled · `B1` + Smart Object claims · `B2` per-agent EQS + claims, event-driven · `SQ` SquadQuery (plus labelled variants such as `SQ-greedy`, `SQ-cap1`, `SQ-nodebug` for Phase 8 sweeps).
- Example: `2026-11-28_101500_B2_N128_r3.csv` (+ `_agents.csv`, `_events.csv`, `.log`), `2026-11-28_101500_manifest.json` in `Raw\`, and `2026-11-28_101500_summary.md` in `Summaries\`.
- `_r0_prime` files are the discarded priming run, kept for transparency and excluded from every summary.

## Environment record (written into every manifest and summary)

```text
Engine:            UE 5.8.4 (CL 58902052), launcher build
Build:             Development, standalone packaged (never PIE)
Compiler:          MSVC 14.44.35229 (VS 2022), Windows SDK 10.0.22621.0
CPU / GPU / RAM:   i7-14650HX (8P + 8E) / RTX 5070 Laptop / 32 GB
CPU affinity:      performance cores only, mask 0xFFFF (logical 0–15): the run script pins itself (pin_self) and every game inherits it; applied_mask() recorded
Storage:           project on D: (NVMe), engine on C: (NVMe)
OS:                Windows 11 build 26200
Power:             AC, power plan recorded per batch
Resolution:        1280x720 windowed, default scalability, no VSync, no sound
EQS:               MaxAllowedTestingTime 0.01 s, breadth first (engine default, restated in DefaultGame.ini); EQS debugger on (Development) unless a batch says otherwise
Command line:      exact, per run, in the manifest
Warm-up excluded:  10 s     Capture: 60 s (10 threat events)     Runs per combination: = number of configurations (interleaved) + 1 priming run per batch
```

## Run log

| Batch | Configs | N | Runs | Purpose | Summary |
|---|---|---|---|---|---|
| — | — | — | — | No benchmark batches yet. The first (the S3 baseline capture) is planned in [../Explanation/03_Implementation_Roadmap.md](../Explanation/03_Implementation_Roadmap.md) | — |
