# 08 · Debugging And Profiling: SquadQuery

**Status:** the tools below exist since S1–S3 and are listed as they are built. S6 adds the `SquadQuery` CSV category, the `sq.Debug` view and Visual Logger lines; the full profiling guide (Unreal Insights drill-downs of an event frame, EQS cycle stats, the Gameplay Debugger's Smart Object category) is written in **Phases 7–8**, with data. Every command here was run on UE 5.8.4.

---

## What you can switch and see (as of S3)

| Tool | Command | What it shows | Built in |
|---|---|---|---|
| Arena self-check | `SquadQuery.exe -nullrhi -nosound -unattended -SQArenaCheck -SQSeed=1 -log` | One `SQArenaCheck PASS/FAIL` line: slots found and registered, location error, slots on the navmesh, corner-to-corner paths, hidden slots per threat position. Exit code 0 or 1 | `05` B5 |
| Arena build | the `LogSQArena` lines of any run | `Arena built: seed …, hash 0x…` (compare hashes only between runs of one binary, `01b` #21), `Navigation built … s (real) after begin play`, `Visibility table: 6000 traces in … ms` | `05` B5 |
| A benchmark run | `python -I -u Tools\run_bench.py --configs Scripted --agents 32 --runs 2 --label NAME` (through the UE queue) | Pinned, interleaved runs of the packaged build; one CSV capture, two run reports and the log per run in `Benchmarks\Raw\`, a summary in `Benchmarks\Summaries\` | `05` C5 |
| Statistics of one capture | `python -I Tools\csv_stats.py CAPTURE.csv [--columns A,B,NAME=A+B]` | Mean, median, p95, p99, max per column; composite columns summed per frame | `05` C5 |
| Event frames of a batch | `python -I Tools\analyse_events.py BATCH` | Per configuration and N: AI mean, p99, **p99.9 and max**; AI inside and outside the event windows (the `SQBench/Threat` frame and the 30 after it); frames over 1 ms; writes `Benchmarks\Summaries\BATCH_events.md` | `05` D8 |
| A baseline on its own | `-SQMode=B0` / `B1` / `B2` with `-SQAgents=N` (or `run_bench.py --configs B0,B1,B2`) | The per-agent EQS baselines (`05` D6) | `05` D7 |
| The run's own summary line | `LogSQBench: Display: BenchEnd after 60.000 s of capture: 10 captured events, 0 conflict onsets, in cover 89.0% of agent-frames` | A quick sanity check without opening any file | `05` C4 |
| Game mode check | `LogLoad: Game class is 'SQGameMode'` | The global game mode applies (no player pawn among the agents) | `05` C2b |

## The CSV capture

Every run captures from `BenchStart` to `BenchEnd` (`-csvStartOnEvent` / `-csvStopOnEvent`). Columns this project reads, with the names exactly as they appear in the file (checked on the first S2 capture):

| Column | What it is |
|---|---|
| `Exclusive/GameThread/EnvQueryManager` | EQS's own exclusive time: query steps and generators. About 0.4 µs per frame even with no query, the manager's tick |
| `Exclusive/GameThread/SquadQueryDecide` | Our decision code (`05` C3), minus anything nested that has its own exclusive scope |
| `Exclusive/GameThread/Pathfinding` | The engine's path queries; every `MoveToLocation` runs one inside `SquadQueryDecide` (`RecastNavMesh.cpp:3839`) |
| `AI` (composite, computed by the script) | The per-frame sum of the three above (`01` §6.3) |
| `SQBench/InCover`, `SQBench/Awaiting`, `SQBench/ConflictSlots` | Per frame: agents in cover, agents still getting into cover for the last event, slots targeted by two or more agents |
| `SQBench/GameThread/Metrics` | The director's metric code. Not part of AI time; the same in every configuration |
| `GameThreadTime`, `FrameTime`, `GPUTime` | Engine frame totals |
| `EVENTS` | `SQBench/BenchStart`, `SQBench/Threat` (one per threat event, so event frames can be found), `SQBench/BenchEnd`; also engine events such as `PSO/…` and `GC` |

How to read them together:
- **AI time on event frames.** Find the frames whose `EVENTS` cell holds `SQBench/Threat`; the event-driven configurations do their work there (H6).
- **`Pathfinding` against move requests.** Each move request is one path query, so `Pathfinding` grows with `move_requests` in the agents report. A configuration that switches more pays here.
- **p99 is blind to event-driven work.** An event-driven configuration works on the frames right after each threat event, under 1% of a capture, so its p99 can look idle while its event frames are the most expensive of all (B2 at N = 128: p99 0.05 ms, event frames up to 8.35 ms, `09` S3). Read p99.9, the maximum and the event-window figures of `analyse_events.py`.
- **Engine events in the capture.** A `GC` event or `PSO/Encountered new … PSO` is a frame-time spike that has nothing to do with decisions; the summary counts PSO events per run.

## The run reports

`<run>_agents.csv` and `<run>_events.csv` start with one `# key=value …` line (mode, agents, seed, arena hash, timing, walking speed, capture totals).

| Symptom | Where to look | What it means |
|---|---|---|
| `claim_failures` high in B2, low in B1 | the agents report | Agents that query on the same frame pick from the same picture and race for the same slots (H5) |
| `query_latency_s` 0 | the agents report | The query finished in the frame it started (world time has 1/60 s resolution); queueing in the EQS slice would show as whole frames |
| `seconds_to_cover = -1` for an agent, event after event | `final_slot_distance_cm` above 75 in the agents report; `move_retries` above 0 | The agent never reached its slot: blocked, usually by another agent (`07` S2) |
| Time to cover 0 for most agent-events | `already_in_cover` in the summary | Their slot stayed valid for the new threat; read the time over the agents that moved |
| Walk ratio well above 1 | `distance_cm` and `seconds_to_cover` in the events report | Decision delay or detours around blocks; the scripted oracle (decides on the event frame) is the reference |
| `conflict_agent_frames > 0` with `conflict_onsets = 0` | the report header | Would mean a conflict started in warm-up and was never counted; fixed in S2 by resetting the state at `BenchStart` |
