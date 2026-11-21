# 03 · Implementation Roadmap: SquadQuery

**Written:** end of Phase 3, 2026-11-21 · **Engine:** UE 5.8.4

**Rules for every step:**
- Each step is small enough to read in one sitting: a few hundred lines at most.
- A step is finished only when its code **compiles with 0 errors and 0 warnings through the queue**, its check **passes**, and its `Explanation/` entries (`05`, `06`, and `07` where relevant) are **written in the same step**.
- After each step that adds C++: run `simplify`, then `code-review`.
- **The baselines are captured before SquadQuery exists.** Steps S1–S3 build only the arena, the agents, the harness and the per-agent baselines (B0, B1, B2). The squad evaluation starts at S4.
- **The riskiest assumption goes first**: S1 checks the runtime navmesh and runtime Smart Objects in a *cooked* build before anything is built on them.

---

**Progress (2026-11-21):** Phase 4 ✅ (F1–F3; checks F-1 to F-6 and the final re-run R-1 to R-8 in `09`). **S1 ✅**: the runtime navmesh and runtime Smart Objects pass in the packaged build (`09` S1-d6), after three failed jobs that the step was designed to catch early (S1-a to S1-c). **S2 ✅**: agents, the director and the scripts, checked on the packaged build (`09` S2-a, S2-b). Next: **S3**.

## Phase 4: Foundation (no gameplay code yet)

| Step | What gets built | How it is checked |
|---|---|---|
| **F1** ✅ | `SquadQuery\SquadQuery.uproject` (EngineAssociation 5.8; plugins `SquadQueryCore` and `SmartObjects`), `Source\SquadQuery.Target.cs`, `SquadQueryEditor.Target.cs`, the game module (`SquadQuery.Build.cs`, `SquadQueryModule.cpp`) | `Tools\build.bat` builds `SquadQueryEditor` and `SquadQuery` (Development) through the queue: 0 errors, 0 warnings |
| **F2** ✅ | The plugin skeleton: `SquadQueryCore.uplugin` (declares its SmartObjects dependency), `SquadQueryCore.Build.cs`, the module `.cpp` | Same builds; the plugin and SmartObjects load in the uncooked game (`smoke_game.bat`) |
| **F3** ✅ | `Config\DefaultEngine.ini` (default map, no smoothing, no VSync, DX12 SM6, `RuntimeGeneration=Dynamic`), `Config\DefaultGame.ini` (basic shapes always cooked; the EQS slice restated); `Tools\build.bat`, `package.bat`, `smoke_game.bat` | `package.bat` produces a packaged Development build; `smoke_game.bat packaged` starts it on the GPU and quits with exit code 0 |

## Phase 5: Implementation

| Step | What gets built | How it is checked | Explanation files touched |
|---|---|---|---|
| **S1. Arena + navigation (risk first)** ✅ | `USQArenaSubsystem` (floor, 100 blocks from the seed, nav bounds volume with a box body, wait for the build, slot table, threat positions, visibility table); `ASQCoverBlock` with a `USmartObjectComponent` and one runtime `USmartObjectDefinition` (6 slots) | In the **packaged** build: navmesh exists and `FindPathSync` succeeds between all four arena corners; the Smart Object subsystem reports 600 slots; the visibility table has ≥ 2 × 128 hidden slots per threat position; the arena hash matches across two runs with one seed and differs with another. **If the runtime volume or definition fails in the cooked build, apply the fallback** (an editor Python script through the queue, `01b` #9, #11) and record it | 04, 05, 06, 07 |
| **S2. Agents + benchmark harness** ✅ | `ASQAgent`; `USQBenchmarkSubsystem` (seeded squad spawn after nav is ready, threat timeline, `BenchStart`/`BenchEnd`, CSV metadata, metrics from the visibility table: conflicts, switches, oscillations, time-to-cover, exposure; run report); a trivial **scripted brain** (each agent walks to a precomputed hidden slot) used only to test the metrics; `Tools\csv_stats.py` and `Tools\run_bench.py` adapted from UE5-UpdateBudget, including `performance_core_mask()`, `pin_self()`, `applied_mask()` and the direct launch of `Build\Windows\SquadQuery\Binaries\Win64\SquadQuery.exe` | One fixed-step packaged run at N = 32 produces a CSV with the `SQBench` and global columns; the scripted brain gives 0 conflicts and a time-to-cover close to distance ÷ speed; the stats script matches a hand calculation; the run's affinity mask is recorded. The `SquadQueryDecide` / `EnvQueryManager` exclusive nesting is checked here (`01b` #5) | 05, 08, 09 |
| **S3. Per-agent baselines B0, B1, B2, captured** | `USquadMemberComponent` and `USquadQuerySubsystem` (registry and threat only, no evaluation), `USquad`, `USquadQueryContext_Threat`; `USQCoverSlotGenerator`; `SQQueries` (the per-agent template); `USQAgentBrainComponent` modes B0, B1, B2 (polling, events, claims, keep-current check, claim-failure re-query); item-test counting from EQS debug data | **The baseline capture**: B0, B1, B2 × N = 8 / 32 / 128 × 3 interleaved runs + priming (runs = number of configurations at this point), fixed step, into `Benchmarks\Raw\`. H1, H3, H4, H5 and H7 are answered for the baselines **before any SquadQuery code** | 04, 05, 06, 07, 09 |
| **S4. The solver + unit tests** | `FSquadAssignmentSolver` (shortest augmenting path; forbidden pairs; "no slot" columns when n > m) | Automation tests: total equals brute force on 1,000 random instances (n ≤ 6, m ≤ 8); forbidden pairs never chosen; n > m; n = 0; all forbidden; ties are deterministic. Solve time measured at 8 × 128 | 04, 05, 06, 09 |
| **S5. Shared evaluation (SQ mode)** | `USquadQueryContext_Members`; the shared template in `SQQueries`; `USquadQuerySubsystem` evaluation (event queue, per-frame cap, generation counter, result handling, cost matrix, claims, notify); `USquadMemberComponent::OnAssignmentChanged`; brain mode SQ | Tests: destroy a member while its squad's query runs; threat moves twice before a result arrives (stale result dropped); two squads want the same slot (no conflict); an empty squad. One SQ run at N = 128: SC4 (0 conflicts) and SC5 checked on a single run before the matrix | 05, 06, 07 |
| **S6. Settings, telemetry, debug view** | `USquadQuerySettings`; the `SquadQuery` CSV category; cvars `sq.Debug`, `sq.Solver`; debug draw (agent → slot lines, slot colours); Visual Logger lines for each assignment | Changing a setting in `DefaultGame.ini` changes the behaviour; the CSV columns exist; the debug view matches the run report on one seed | 05, 06, 08 |
| **S7. The headline matrix** | No new code. B0, B1, B2, SQ × N = 8 / 32 / 128 × 4 interleaved runs + priming | SC1–SC7 evaluated in `09`. Then the Phase 8 extras (`11`) | 09 |
| **S8. (Optional) StateTree adapter** | The member component sends one `SendStateTreeEvent` per assignment change when its owner has a `UStateTreeComponent`; a small C++ StateTree task that moves to the assigned slot; a demo StateTree asset, created with an editor Python script through the queue (or by hand if the asset can't be scripted; the outcome is recorded) | A demo squad driven by StateTree behaves like the C++ brain on one seed; its extra cost per event is measured, not assumed | 05, 06, 07 |

## Phases 6–10 (after the code exists)

| Phase | Work |
|---|---|
| 6 Explanation | Consolidate 04–07 into one consistent line-by-line pass; re-check every API against 5.8.4 |
| 7 Testing | Edge cases (0 and 1 agents, a squad of 1, more agents than hidden slots, all slots claimed, threat on top of a squad, agents destroyed mid-query), stability (a 30-minute soak with no claim leaks: claims held = agents with targets), scalability (N sweep) |
| 8 Profiling | The matrix from S7, plus: EQS debugger off (`bDebuggingInfoEnabled = false`) for B2 and SQ at N = 128; `MaxEvaluationsPerFrame` 1 / 4 / 16; `sq.Solver=Greedy` ablation; one uncapped real-time confirmation run; Insights drill-down of an event frame. Optimisations only with data (`11`) |
| 9 Code review | `code-review` and `simplify`, plus a written self-review: weaknesses, not praise |
| 10 Portfolio | Charts (`dataviz`), an architecture page, the demo video (ffmpeg capture script from the main session), README results, interview Q&A |

**Rough matrix cost:** 48 runs × (about 15 s start + 3 s nav build + 10 s warm-up + 60 s capture) ≈ 70–80 minutes, scripted, unattended, through the queue.
