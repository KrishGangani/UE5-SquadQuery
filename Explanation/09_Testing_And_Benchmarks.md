# 09 · Testing And Benchmarks: SquadQuery

**Rule:** every check is recorded with its date, its exact command and its result, **including failures**. Every Unreal job ran through the machine-wide queue (`tools\ue_queue.py`, outside this repository); the job id is given so the full log can be found. The functional and benchmark sections are filled in during Phases 5–8.

---

## Phase 4 · Foundation checks (2026-11-21)

**Job `20261121_011304_c2df96`** ("P07 Phase 4: build editor + game (Development), uncooked smoke, package Development, packaged smoke (GPU)"): exit code 0 in 193.8 s, with the queue's `--timeout 7200 --idle 1500`. Command, run from the portfolio root:

```text
"…\P07_SquadQuery\Tools\build.bat" SquadQueryEditor Development && "…\Tools\build.bat" SquadQuery Development
&& "…\Tools\smoke_game.bat" && "…\Tools\package.bat" Development && "…\Tools\smoke_game.bat" packaged
```

| # | Check | Command (from `P07_SquadQuery\`) | Result |
|---|---|---|---|
| F-1 | Editor target builds | `Tools\build.bat` (`SquadQueryEditor Win64 Development`) | ✅ `Result: Succeeded` in 37.2 s; 12 actions (both modules compiled and linked as `UnrealEditor-SquadQuery.dll` and `UnrealEditor-SquadQueryCore.dll`); **0 errors, 0 warnings**. Toolchain: MSVC 14.44.35229, Windows SDK 10.0.22621.0 |
| F-2 | Game target builds | `Tools\build.bat SquadQuery Development` | ✅ `Result: Succeeded` in 37.6 s; 24 actions, `SquadQuery.exe` linked; **0 errors, 0 warnings** |
| F-3 | Uncooked game loads both modules, SmartObjects and the map, then quits | `Tools\smoke_game.bat` (`UnrealEditor-Cmd -game -nullrhi … -ExecCmds="quit"`) | ✅ Exit 0. `Mounting Engine plugin SmartObjects`, `Mounting Project plugin SquadQueryCore`, `SquadQueryCore` DLL loaded; `Template_Default` loaded in 0.047 s. **0 errors.** 21 warnings, all `LogEditorDataStorageUI: Unable to register widget factory …`: the editor binary's own UI modules in `-game` mode, the same engine-side warnings UE5-UpdateBudget recorded (its `09` F-2). None from our modules, SmartObjects, navigation or input |
| F-4 | Package a **Development** build | `Tools\package.bat` | ✅ `BUILD SUCCESSFUL`; BuildCookRun 63.5 s (cook 25.6 s, stage 34.8 s); 526 packages cooked; cook summary `Success - 0 error(s), 0 warning(s)`. Output `Build\Windows\` (991 MB staged, git-ignored). The real game executable is `Build\Windows\SquadQuery\Binaries\Win64\SquadQuery.exe`, the one the benchmark script will launch directly, pinned to the performance cores by inheritance (`01` §7) |
| F-5 | Packaged build runs on the GPU at the benchmark resolution | `Tools\smoke_game.bat packaged` (`-windowed -ResX=1280 -ResY=720 -nosound -unattended -traceautostart=0 -ExecCmds="quit"`) | ✅ Exit 0; D3D12, SM6; **chosen adapter 0 = NVIDIA GeForce RTX 5070 Laptop GPU** (driver 610.47), not the integrated Intel GPU; map loaded in 0.124 s; `LogEnhancedInput: … has initialized the user settings!` (the `DefaultInput.ini` fix works, `05` A9); **0 errors, 0 warnings** |
| F-6 | Test configuration | not attempted | The launcher engine can't build Test (UE5-UpdateBudget `09` F-3a). Headline builds are packaged Development (`01` §7) |
| F-7 | The config keys reach the right classes | moved into the re-run below (R-4, R-7) | The queued stand-alone job was cancelled when the `.uproject` changed, so the check runs on the final configuration |

### Phase 4 re-run: Android File Server disabled (2026-11-21)

**Why.** The first cook showed that an engine plugin wrote a generated security token into `DefaultEngine.ini` (observation 1 below). The portfolio rule is to disable that plugin, delete its section, and keep the docs in step. Changing the `.uproject` changes the module set, so every Phase 4 check was repeated on the final configuration.

**Job `20261121_015015_186b6d`** ("P07 Phase 4 re-run (AndroidFileServer disabled): build editor + game, uncooked smoke + config check, package Development, packaged smoke (GPU) + config check"): exit code 0 in 111.8 s. The old `Build\` folder was deleted first, so the package is fresh.

| # | Check | Command (from `P07_SquadQuery\`) | Result |
|---|---|---|---|
| R-1 | Editor target builds | `Tools\build.bat` | ✅ `Result: Succeeded` in 11.1 s; 4 actions (UBT rebuilt only what the plugin change touched); **0 errors, 0 warnings** |
| R-2 | Game target builds | `Tools\build.bat SquadQuery Development` | ✅ `Result: Succeeded` in 18.5 s; `SquadQuery.exe` re-linked without the Android File Server modules; **0 errors, 0 warnings** |
| R-3 | Uncooked smoke | `Tools\smoke_game.bat` | ✅ Exit 0. SmartObjects and SquadQueryCore mounted; **Android File Server not mounted**; **0 errors**; the same 21 `LogEditorDataStorageUI` editor-binary warnings as F-3 and nothing else |
| R-4 | Config check, uncooked | `UnrealEditor-Cmd … -game -nullrhi … -ExecCmds="getall RecastNavMesh RuntimeGeneration showdefaults, getall NavigationSystemV1 bAutoCreateNavigationData showdefaults, getall EnvQueryManager MaxAllowedTestingTime showdefaults, getall EnvQueryManager bTestQueriesUsingBreadth showdefaults, quit"` | ✅ `Default__RecastNavMesh.RuntimeGeneration = Dynamic` (`BaseEngine.ini` sets `Static` for `NavigationData`, so this proves our `RecastNavMesh` section is the one applied); `bAutoCreateNavigationData = True` on the default object and on the world's navigation system; `MaxAllowedTestingTime = 0.010000` and `bTestQueriesUsingBreadth = True` on the default object and on the live `EnvironmentQueryManager`. No `RecastNavMesh` instance exists yet: there are no navigation bounds until the S1 arena spawns them, as expected |
| R-5 | Package a **Development** build | `Tools\package.bat` | ✅ `BUILD SUCCESSFUL`; BuildCookRun 57.6 s (cook 19.9 s, stage 34.4 s); 526 packages cooked; cook summary `Success - 0 error(s), 0 warning(s)` |
| R-6 | Packaged build on the GPU, 1280×720 | `Tools\smoke_game.bat packaged` | ✅ Exit 0; adapter 0 = RTX 5070 Laptop GPU (driver 610.47); map loaded in 0.089 s; Android File Server not mounted; **0 errors, 0 warnings**. Shutdown took 0.8 s (first run: about 30 s, observation 2) |
| R-7 | Config check, packaged | `Build\Windows\SquadQuery.exe -nullrhi … -ExecCmds="getall … (same four), quit"` | ✅ The same values as R-4, read from the **cooked** config: `RuntimeGeneration = Dynamic`, `bAutoCreateNavigationData = True`, `MaxAllowedTestingTime = 0.010000`, `bTestQueriesUsingBreadth = True`; **0 errors, 0 warnings** |
| R-8 | Nothing rewrites the config | `git diff SquadQuery/Config` after the job | ✅ The only change to `DefaultEngine.ini` is the deleted Android File Server section. The build, the cook and three game runs wrote nothing back |

**Observations from the Phase 4 runs:**
1. **The editor wrote into `DefaultEngine.ini` during the first cook.** The Android File Server plugin (enabled by default, `Plugins/Runtime/AndroidFileServer/AndroidFileServer.uplugin:15`) generates a random security token the first time its settings load with the placeholder `[AUTO]`, and saves the whole section (`Plugins/Runtime/AndroidFileServer/Source/AndroidFileServerEditor/Private/AndroidFileServerRuntimeSettings.cpp:27-37`). The generated token never reached a commit: an intermediate fix kept the section with an empty token (commit `88564dd`); the final fix disables the plugin in the `.uproject` and deletes the section (`05` A1, A7; verified by R-3, R-6, R-8).
2. **The first packaged run took about 30 s to shut down**: 29.6 s passed between `Slate User Destroyed` and `LogD3D12RHI: Waited for PSO creation for 100.000000ms`. The second packaged run (R-6) shut down in 0.8 s. So the wait was most likely pipeline-state compilation on the first run of a fresh build **[inferred from the log line and the difference between the two runs]**. It falls outside any capture window, but the run script must allow for it in its timeout, and the discarded priming run of each batch absorbs it.
3. **The uncooked and packaged starts both mount SmartObjects and its dependencies without a warning.** The launcher engine provides everything this project needs at Phase 4: no source-only plugin and no extra target type.
4. **`-ExitAfterSeconds` is not used** in any script: 5.8.4 doesn't parse it (`05` A10). Every run ends with `-ExecCmds="quit"`.

---

## S1 · Arena, Smart Objects and runtime navigation in a cooked build (2026-11-21)

**What S1 had to prove** (`03` S1, the riskiest assumption first): that the arena built entirely in code works in a **packaged** build. That means a navmesh generated at runtime for runtime-spawned geometry, runtime Smart Object definitions whose slots register, a visibility table with enough hidden slots, and a layout that depends only on the seed. The check is built into the game: `-SQArenaCheck` builds the arena, runs every test once the navmesh is ready, logs one `SQArenaCheck PASS` or `FAIL` line, and quits with exit code 0 or 1. The queue job fails if any run fails.

**The job command** (one queue job; every step must succeed for the next to run):

```text
Tools\build.bat SquadQueryEditor Development && Tools\build.bat SquadQuery Development
&& UnrealEditor-Cmd SquadQuery.uproject -game -nullrhi -nosound -unattended -nosplash -traceautostart=0 -SQArenaCheck -log -SQSeed=1 -abslog=…\S1_uncooked_seed1_a.log
&& (the same, -abslog=…\S1_uncooked_seed1_b.log) && (the same with -SQSeed=2, -abslog=…\S1_uncooked_seed2.log)
&& Tools\package.bat Development
&& Build\Windows\SquadQuery\Binaries\Win64\SquadQuery.exe -nullrhi -nosound -unattended -traceautostart=0 -SQArenaCheck -log -SQSeed=1 -abslog=…\S1_packaged_seed1.log
&& Tools\smoke_game.bat packaged
```

| # | Job | Result |
|---|---|---|
| S1-a | `20261121_025958_efa3dc` | ❌ exit 6 in 11.5 s: the editor target didn't compile. `SQArenaSubsystem.cpp(342): error C2027: use of undefined type 'FSmartObjectRequestFilter'` (and `'FSmartObjectRequest'`). `SmartObjectSubsystem.h` only forward-declares both (`SO/Public/SmartObjectSubsystem.h:25, 27`). **Fix:** include `SmartObjectRequestTypes.h`, which defines them (`05` B5; `07` S1) |
| S1-b | `20261121_031527_1cf6b4` | ❌ exit 6 in 14.4 s: compiled, but `UnrealEditor-SquadQuery.dll` didn't link. Three unresolved externals: `FGameplayTagContainer::operator=`, `FGameplayTagQuery::FGameplayTagQuery()` and its copy constructor, used by the request filter copy in the self-check and by the slot definitions `DebugAddSlot` constructs. **Fix:** `GameplayTags` added to the game module's private dependencies (`05` B1; `07` S1). `SmartObjectsModule` lists it as a public dependency, but that was not enough to link **[observed]** |
| S1-c | `20261121_034337_cc3f77` | ❌ exit 1 in 81.6 s. Both targets built (**0 errors, 0 warnings**); the first uncooked self-check failed: `SQArenaCheck FAIL: seed 1 hash 0x0EBBDFD8; slots 600 (expected 600), found by the Smart Object runtime 600, with handle 600, max location error 0.000 cm, on navmesh 0; complete corner paths 0/4; threat positions 10; hidden slots per threat 303..363, of them >= 8 m from the threat at least 301 (needs 256)`. So the runtime Smart Objects, the slot table and the visibility table already worked; **the navmesh didn't**. The line before it, `LogNavigationDirtyArea: Warning: Skipped some dirty area creation due to: 0 invalid bounds, 1 empty bounds (element: None, from: Navigation bounds update)`, gave the cause: the bounds volume had registered during the spawn, before it got its box body, and its **cached** bounds were never recomputed, so the navigation system was told about a point (`01b` #20). This run also had the review's event-driven readiness without the path test (audit below), and it reported `Navigation built 0.376 s` on frame 1: the build-finished event had fired for that near-empty build. **Fixes:** `Brush->UpdateBounds()` after assigning the body; readiness = event + nothing queued + a corner-to-corner path (`05` B5; `07` S1). This is the risk S1 exists to catch (`03`): found on the first run that reached the check |
| S1-d | `20261121_040748_923864` | ✅ exit 0 in 192.8 s; the checks are below |

**S1-d, the passing job** (2026-11-21; editor build 33.6 s, game build 40.5 s, BuildCookRun 66.4 s):

| # | Check | Result |
|---|---|---|
| S1-d1 | Both targets build | ✅ `Result: Succeeded` twice; **0 errors, 0 warnings** |
| S1-d2 | Uncooked self-check, seed 1, run a | ✅ `SQArenaCheck PASS: seed 1 hash 0x0EBBDFD8; slots 600 (expected 600), found by the Smart Object runtime 600, with handle 600, max location error 0.000 cm, on navmesh 600; complete corner paths 4/4; threat positions 10; hidden slots per threat 303..363, of them >= 8 m from the threat at least 301 (needs 256)`. Navigation built 0.511 s (real) after begin play, on frame 29 |
| S1-d3 | Uncooked self-check, seed 1, run b | ✅ The same line, the **same hash 0x0EBBDFD8**; navigation built in 0.299 s (frame 24) |
| S1-d4 | Uncooked self-check, seed 2 | ✅ PASS, hash **0x93B88D51** (different, as it must be); hidden slots per threat 298..360, at least 296 of them ≥ 8 m away; navigation 0.305 s |
| S1-d5 | Package | ✅ `BUILD SUCCESSFUL`; 519 packages cooked; cook summary `Success - 0 error(s), 0 warning(s)` |
| S1-d6 | **Packaged** self-check, seed 1 | ✅ **PASS** with every count identical to S1-d2: 600 slots found and registered, 0.000 cm location error, 600 on the navmesh, 4/4 corner paths, 303..363 hidden, at least 301 usable. Navigation built **0.047 s** after begin play (frame 20). **The two risky assumptions hold in a cooked build:** a runtime navmesh over runtime-spawned geometry, and runtime Smart Object definitions whose slots register. Hash **0x1FFC2AF6** (see observation 1) |
| S1-d7 | Packaged smoke on the GPU (seed 1, the default) | ✅ Exit 0; the arena built again with hash **0x1FFC2AF6**, so the packaged build also repeats its own layout bit for bit |

**Observations from S1:**
1. **The editor binary and the game binary give different hashes for the same seed** (0x0EBBDFD8 vs 0x1FFC2AF6), while every count that depends on the layout is identical (hidden slots 303..363, at least 301 usable, 600 on the navmesh). Cause: with `BuildSettingsVersion.V7`, UBT compiles **Editor** targets with `/fp:precise` and **Game** targets with `/fp:fast` (`Programs/UnrealBuildTool/Platform/Windows/VCToolChain.cs:1324-1335`; `Configuration/Rules/TargetRules.cs:3737-3740`), so the same float expressions may round differently in the last bit (`01b` #21). The layout is the same to within rounding; bit-exact equality holds **within one binary** (S1-d2 = S1-d3, S1-d6 = S1-d7). Consequence: hashes are compared only between runs of the same binary, and every measured run uses the packaged one.
2. **One runtime warning at teardown:** `LogCrowdFollowing: Warning: Unable to find RecastNavMesh instance while trying to create UCrowdManager instance` (`AIModule/Private/Navigation/CrowdManager.cpp:905`), during `BeginTearingDown`, in every run that built the navmesh (the Phase 4 smoke had no navmesh and no warning). Nothing here uses Detour crowd following (agents use the AI controller's path following), so S2 removes the crowd manager in config and checks the warning is gone; UE5-NavReadyPCG met the same warning and fixed it the same way.
3. **Navigation is fast:** 0.05 s after begin play in the packaged build, 0.3–0.5 s in the editor binary. Agents spawn only after it (S2), so no navmesh build falls inside a capture.
4. **The cover budget is comfortable:** at least 296–301 hidden slots ≥ 8 m from every threat position, against 256 needed for 128 agents.

### S1 audit trail (before the commit)

**1. `code-review` (medium) on the S1 source** (`Source\SquadQuery\`: `SQArenaSubsystem.h/.cpp`, `SQCoverBlock.h/.cpp`, `SquadQuery.Build.cs`). No blocking bug; six minor findings:

| # | Finding | Decision |
|---|---|---|
| R1 | The arena could become ready with no Smart Object handles (a definition that fails validation registers nothing, silently), and only `-SQArenaCheck` would notice | **Fixed:** `FillSlotHandles` counts valid handles and logs an Error in every run when the count isn't 600 (`05` B5) |
| R2 | `FillSlotHandles` dereferenced `Blocks[i]` and its component without a null check | **Fixed:** a missing block or component is skipped |
| R3 | `IsSlotHidden` indexed the table with no ready or range check | **Fixed:** returns false before ready or out of range (`05` B4) |
| R4 | Threats are kept 2.6 m from block centres, not from slots, so a short-side slot can be about 0.5 m from a threat | **Not changed:** a slot within 8 m of the threat never counts as cover, in the query (distance filter) or in the metrics, so no agent is ever sent there; the clearance only keeps the threat's eye out of a block |
| R5 | With no threat positions the self-check line would print `2147483647` as the minimum | **Fixed:** the minimums start at 0 in that case |
| R6 | A missing engine cube would leave the floor without a mesh, with nothing logged | **Fixed:** an Error line (`05` B5) |

**2. `simplify` on the same code** (four reviewers: reuse, simplification, efficiency, altitude). Applied:

| Angle | Change |
|---|---|
| Reuse | `FCrc::TypeCrc32` replaces the hand-written `HashVector` (bit-identical hash); `TestPathSync` replaces `FindPathToLocationSynchronously` (no `UNavigationPath` object per test); the cube's path and size are one constant each on `ASQCoverBlock` |
| Simplification | `NumBlocks`, `NumSlots`, `GridHalfExtentCm` derived once; `CornerCm` derived from them with a `static_assert` (`04` §10); one `RequestSelfCheckExit` for both exit paths; the four self-check path tests from one table (`04` §11); the redundant `StartSeconds`; the self-check box height from the bounds constant; an accurate comment on who keeps the definition alive |
| Efficiency | Nothing on a hot path. `IsSlotHidden` stays inline (a few ns per call) |
| Altitude | Readiness from the engine's build-finished event instead of a 0.1 s poll, with a one-shot real-time timeout; the floor and the bounds volume spawned with a plain `SpawnActor`, because a deferred spawn doesn't delay registration for actors with a native root (the old comments said it did); `02` §8 now points to the single readiness rule in §4.2 |

Not applied: computing `BlockIndex`/`SlotIndex` from the table index and filling handles per block (the explicit fields keep the table correct if a block fails to spawn); a second "hidden and ≥ 8 m" bit table (not needed until a measured path uses it).

**3. The first self-check run then corrected the altitude change** (S1-c): the event alone was not enough, so readiness keeps one path test per event (`07` S1). The passing job S1-d ran the final code, with every change above in it.

---

## S2 · Agents, the benchmark director and the measurement scripts (2026-11-21)

**What S2 had to show** (`03` S2): one fixed-step packaged run at N = 32 produces a CSV with the `SQBench` and global columns; the scripted test oracle gives 0 conflicts and a time to cover close to distance ÷ speed; the statistics script matches a hand calculation; the run's affinity mask is recorded. The `SquadQueryDecide` / `EnvQueryManager` nesting check needs EQS queries, so it moves to S3, where the first queries run; the nesting of the engine's own `Pathfinding` scope inside ours is checked here.

| # | Job | Result |
|---|---|---|
| S2-a | `20261121_052141_d75641` | ✅ exit 0 in 143.2 s. Both targets built with **0 errors, 0 warnings**; cook `Success - 0 error(s), 0 warning(s)`. Packaged arena check PASS with the same counts as S1-d6 and **0 warnings** (the crowd-manager warning of S1 is gone); `LogLoad: Game class is 'SQGameMode'`. Batch `2026-11-21_061758_S2check`: priming run + 1 run, Scripted, N = 32, both exit 0, 0 log errors. The run revealed stuck agents (below), so S2 was run again |
| S2-b | `20261121_062200_cef0bb` | ✅ exit 0 in 163.7 s. Both targets built with **0 errors, 0 warnings**; cook `Success - 0 error(s), 0 warning(s)`. Batch `2026-11-21_071205_S2check2`: priming run + 2 runs, Scripted, N = 32, all exit 0, **0 warnings and 0 errors** in every run log |

**S2-a, the checks:**

| # | Check | Result |
|---|---|---|
| S2-a1 | The capture exists and is complete | ✅ 3,600 frames (60 s at the fixed 60 Hz step), finalised; events: 10 × `SQBench/Threat`, `SQBench/BenchEnd`, 2 × `PSO`, 1 × `GC` |
| S2-a2 | The columns exist | ✅ `Exclusive/GameThread/EnvQueryManager`, `…/SquadQueryDecide`, `…/Pathfinding`, `SQBench/InCover`, `SQBench/Awaiting`, `SQBench/ConflictSlots`, `SQBench/GameThread/Metrics` (the timing stat's real column name), `GameThreadTime`, `FrameTime` |
| S2-a3 | **The engine's path query nests inside ours** | ✅ `Pathfinding` is non-zero exactly on event frames (max 0.122 ms) while `SquadQueryDecide` peaks at 0.142 ms: the path query's time is recorded separately, which is why AI time adds it (`05` C5). `EnvQueryManager` costs 0.4 µs per frame with no query at all |
| S2-a4 | **The statistics script matches a hand calculation** | ✅ An independent calculation from the raw CSV (per-frame sum of the three AI columns; linear-interpolation percentiles written out by hand) gives AI mean 0.001151 ms, AI p99 0.001701 ms, `GameThreadTime` median 1.61435 ms and p95 2.23627 ms: identical to the manifest to the last digit |
| S2-a5 | The affinity mask is recorded | ✅ `cpu_affinity: 0xFFFF (performance cores only; hybrid CPU)` in the environment record |
| S2-a6 | **The oracle: 0 conflicts** | ✅ `conflict_onsets = 0`, `conflict_agent_frames = 0` |
| S2-a7 | **The oracle: time to cover ≈ distance ÷ speed** | ✅ Walk ratio median **1.00** (p95 1.90) over the agents that walked at least 1 m: an agent that decides on the event frame and walks straight arrives in distance ÷ 450 cm/s. The p95 is the detours around blocks |
| S2-a8 | Every agent reaches cover | ❌ **19 of 320 agent-events (5.9%) not in cover before the next event**, with 16 move retries. Every stuck agent stood with its capsule against another agent's (centres 68–78 cm apart; the capsule radius is 34 cm): path following plans around blocks, not around agents, so an agent whose path ran through a squadmate already in a neighbouring slot pushed into it until the move ended blocked, and the retry met the same obstacle. **Fix:** RVO avoidance on the agents' character movement (`05` C2; `07` S2) |
| S2-a9 | Time to cover is informative | ❌ Median **0.00 s**: 213 of 320 agent-events (67%) were already in cover, because their slot stayed valid for the new threat. Over the 88 that moved, the median was 0.83 s and the p95 2.95 s (median walk 2.3 m). **Fix:** the summary gives the share already in cover and the time to cover of the agents that moved (`05` C5) |

Other numbers from S2-a (scripted, N = 32; for orientation only, not a benchmark result): `GameThreadTime` median 1.61 ms; `FrameTime` median 2.98 ms (GPU, 1280 × 720); 0.32 switches per agent per event; in cover 89% of agent-frames; 94% of agent-events in cover before the next event. The fixed step makes the game run faster than real time: 60 s of game time took about 11 s of wall time.

**S2-b, the checks** (with RVO avoidance and the new report fields):

| # | Check | Result |
|---|---|---|
| S2-b1 | Captures complete, columns present | ✅ 3,600 frames each, finalised; 10 × `SQBench/Threat` per capture; the same columns as S2-a2 |
| S2-b2 | The oracle: 0 conflicts | ✅ 0 onsets, 0 conflict agent-frames in both runs |
| S2-b3 | The oracle: time to cover ≈ distance ÷ speed | ✅ Walk ratio median **1.14** (p95 3.06), up from 1.00 without avoidance: steering around other agents adds detours, the same for every configuration |
| S2-b4 | **Every agent reaches cover** | ✅ **Stuck at end: 0** (largest final distance to a slot 50 cm, the acceptance radius); 2 move retries; **98.8%** of agent-events in cover before the next event (S2-a: 94.1%). The 4 remaining misses are single long walks in a 6 s window, not stuck agents |
| S2-b5 | The time-to-cover figures | ✅ Already in cover 67.5%; agents that moved: median **1.06 s**, p95 3.11 s; in cover 91.6% of agent-frames |
| S2-b6 | **Repeatability** | ✅ Both runs: arena hash `0x1FFC2AF6`, start-position hash `0x0BC51486`, and every decision metric identical (the oracle has no randomness and the step is fixed). Cost figures vary slightly between the runs, as they must: `GameThreadTime` median 1.513 / 1.548 ms, `FrameTime` median 2.953 / 2.995 ms |
| S2-b7 | Affinity recorded, pinned launch | ✅ `cpu_affinity: 0xFFFF (performance cores only; hybrid CPU)`; the script pinned itself before recording the environment and launched `SquadQuery.exe` directly |

**Observations from S2:**
1. **The fixed step runs faster than real time:** 60 s of game time in about 11 s of wall time at N = 32 with the scripted oracle. Wall time per run will grow with the cost of the configuration, not with the capture length.
2. **Every capture has two PSO events and one garbage collection**, at the same places in every run. They are frame-time spikes outside AI time and the same for every configuration; the summary counts PSO events.
3. **`EnvQueryManager` costs about 0.4 µs per frame with no query**: the manager's own tick, present in every configuration.
4. **What S3 inherits:** the `SquadQueryDecide` / `EnvQueryManager` nesting check (S2 has no queries), and the scripted oracle's numbers as the reference for decision delay (walk ratio 1.14, moved time to cover 1.06 s at N = 32).

### S2 audit trail (before the build)

The review and the simplify pass ran **before** the first S2 build, so the job below built the final code.

**1. `code-review` (medium) on the S2 diff** (agents, brain, director, `Build.cs`, `DefaultEngine.ini`, both scripts). Seven findings:

| # | Finding | Decision |
|---|---|---|
| R1 | The spawn height came from `GetSimpleCollisionHalfHeight()` on the class default object, which returns 0 there: every agent would have spawned 86 cm into the floor (`Character.cpp:234-239`) | **Fixed:** `GetDefaultHalfHeight()` (`05` C4; `07` S2) |
| R2 | AI time left out path finding: the engine's `FindPath` opens its own exclusive CSV scope inside `SquadQueryDecide` (`RecastNavMesh.cpp:3839`), so the cost of the path query every switch starts was subtracted and never added back | **Fixed:** AI = `EnvQueryManager` + `SquadQueryDecide` + `Pathfinding` (`05` C5; `01` §6.3 and `02` §9 updated) |
| R3 | An agent whose move failed or ended blocked never moved again while its slot stayed valid | **Fixed**, then fixed again by the simplify pass (below, A1) |
| R4 | Oscillations were not detected across a stop (A → none → A) | **Fixed:** stops are transparent (`05` C3) |
| R5 | A conflict already running at `BenchStart` was never counted as an onset | **Fixed:** the per-slot conflict state is reset at `BenchStart` |
| R6 | Any event interval > 0 was accepted, though at most one event fires per frame | **Fixed:** at least 1 s |
| R7 | A `--label` with spaces broke the run tag | **Fixed:** letters, digits, `_` and `-` only |

**2. `simplify` on the reviewed diff** (four reviewers: reuse, simplification, efficiency, altitude). Applied:

| # | Angle | Change |
|---|---|---|
| A1 | Altitude | The R3 fix re-issued a lost move only when a chooser picked the same slot again, so the retry rate depended on the configuration (every 0.5 s for the polled baselines, never for the event-driven ones). Moves are now retried from the path-following component's `OnRequestFinished` event, identically in every configuration (`05` C3) |
| A2 | Altitude | `ASQGameMode` (no player pawn) replaces two workarounds: destroying the template map's default pawn in the director, and re-applying the camera every frame. The camera is now set once with `bAutoManageActiveCameraTarget = false` (`05` C2b, C4) |
| A3 | Altitude | The summary's definition of AI time is generated from `AI_COLUMN`; it had already drifted once |
| A4 | Reuse | One "valid cover" rule, `USQArenaSubsystem::IsValidCover`, and one 8 m constant on the arena class, used by the self-check and the director (and the S3 queries) |
| A5 | Reuse | The director reads the seed from the arena instead of parsing `-SQSeed=` a second time |
| A6 | Simplification | One `FSQBrainCounters` struct with `operator-` replaces four parallel `…AtStart` fields; one `FArrival` array replaces two parallel arrays; the wait's start time is the event's own time; agent-frames are computed, not counted; a captured-event counter replaces two `FilterByPredicate(…).Num()` copies; the exit latch uses the frame counter; unused `AgentIndex` and `GetCurrentThreatIndex` removed |
| A7 | Simplification | `run_bench.py` uses `csv_stats.DEFAULT_COLUMNS` instead of a second column list |
| A8 | Efficiency | The brain caches its controller; the director reserves its event array. Nothing else was on a hot path |

Not applied, with the reason: the threat-mode string compare (S3 replaces it with an enum, `ESQBrainMode`); the O(agents × slots) scan in the scripted oracle (it is a test oracle, not a measured configuration); the O(slots) conflict scan per frame (about 1 µs, the same in every configuration); hoisting the events-column lookup and removing `--skip` in `csv_stats.py` (between runs only; kept identical to UE5-UpdateBudget's tested script).

**3. Changes after the first run, self-reviewed** (the S2-a findings; small enough that a second skill pass would review three lines):
- *Correctness:* `bUseRVOAvoidance` is a public flag set in the constructor, read by character movement when it registers; the final slot distance uses the same `Dist2D` as the metrics; the new script metrics are pure functions of the two reports, and "already in cover" (0 s) and "moved" (> 0 s) split the reached samples exactly. S2-b exercised all three.
- *Reuse:* `IN_COVER_RADIUS_CM = 75` in `run_bench.py` mirrors `USQBenchmarkSubsystem::InCoverRadiusCm`. A cross-language copy, kept with a comment pointing at the C++ constant; the walking speed already travels in the report header, and the radius can join it if it ever changes.
- *Simplification, efficiency, altitude:* nothing to change; avoidance is the engine's own mechanism for the problem, applied once in the agent class rather than as retries in the brain.

---

## Phases 5–8 (to be written)

- S3: the baseline capture; S4: solver tests; S5: SQ checks; S7: the headline matrix and SC1–SC7.
