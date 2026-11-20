# 01b · Research Findings (Phase 2): SquadQuery

**Phase:** 2 (Research), completed 2026-11-21 · **Engine:** UE 5.8.4 (CL 58902052), read from `C:\Program Files\Epic Games\UE_5.8\Engine`

**Method:**
- Four investigations: how EQS runs and caches queries; Smart Object claims, StateTree events and perception; navigation for runtime-built geometry plus measurement; existing work on the web.
- Every engine claim cites a path relative to `Engine/` and a line number in 5.8.4. `AIModule` = `Source/Runtime/AIModule`, `SO` = `Plugins/Runtime/SmartObjects/Source/SmartObjectsModule`.
- The claims that drive design decisions were **re-opened by hand** before being written here; they are marked ✔.
- **[inferred]** = drawn from following the code, not a direct statement. **[to measure]** = a hypothesis that needs data. **[to verify in S1]** = checked by the first runtime step.

**Why this phase mattered here.** Two findings changed the project's framing, not just its details: a per-agent single-result EQS query is much cheaper than the plan assumed (#3), and the core idea already exists in a 2014 forum design and in F.E.A.R. (#13). The project is narrowed to what is still unsolved: an engine-native implementation and a controlled study of cost **and** decision quality.

---

## 1. What Phase 2 changed

| # | Finding | Consequence for SquadQuery |
|---|---|---|
| 1 | **EQS runs every query as an independent instance per querier, inside one global time slice.** `UEnvQueryManager` is a tickable; `RunQuery` only queues; `Tick` steps queries breadth-first until `MaxAllowedTestingTime` is spent ✔ | Cost scales with the number of *queries*, and when the slice is full, queries **wait** for later frames. Query latency (request → result) becomes a measured quantity (H7) |
| 2 | **The shipped slice is 10 ms per frame, breadth first** (`BaseGame.ini:230-232`; constructor `EnvQueryManager.cpp:182-183`). The config struct's `0.003` is only used through `Configure` ✔ | Every configuration runs under the engine default. `DefaultGame.ini` restates it so it can't change silently. 10 ms is 60% of a 60 fps frame, so EQS can produce real spikes |
| 3 | **A `SingleResult` query moves its most expensive filter last and stops at the first item that passes** (`EnvQueryManager.cpp:887, 906-912, 946-953`; `EnvQueryInstance.cpp:433-453, 614-627`) ✔ | **Corrects the README.** A per-agent "nearest hidden cover" query traces only until it finds one hidden item, not every item. A shared squad query must return *all* valid items, so it traces all of them. Sharing is **not automatically cheaper** (new hypothesis H2). The squad query's candidate set must be no larger than needed (#10) |
| 4 | **EQS caches templates, not results.** `FEnvQueryInstanceCache` holds a duplicated template and a prototype instance keyed by name and run mode; every request copies the prototype (`EnvQueryManager.h:128-144`; `EnvQueryManager.cpp:792-800, 966`) ✔ | The README's claim is **confirmed**. Nothing in 5.8.4 reuses one querier's items or scores for another |
| 5 | **The finish callback's time is excluded from the EQS slice** (`EnvQueryManager.cpp:500-501, 575-577`); the whole tick is a CSV exclusive stat (`:440`) that appears as `Exclusive/GameThread/EnvQueryManager` in packaged captures on this machine ✔ | "AI ms" is measured as that engine column **plus** our own exclusive stat around all decision code, including the code inside finish callbacks, so nothing is counted twice ([inferred], checked in S2) |
| 6 | **Development builds run the EQS debugger on every query**: `USE_EQS_DEBUGGER` is on outside Shipping/Test, and `bDebuggingInfoEnabled` defaults to true, so every step stores time and item counts and every finished query is stored (`EnvQueryTypes.h:33, 1315-1318`; `EnvQueryInstance.cpp:107, 482-485`; `EnvQueryManager.cpp:508-511`) ✔ | (a) Headline builds (packaged Development; the launcher engine can't build Test) include a **per-query** overhead that Shipping doesn't. It favours configurations that run fewer queries, so Phase 8 measures it separately by switching `bDebuggingInfoEnabled` off. (b) The same data gives an **exact item-test count** per query (`NumProcessedItems` per step), which is how "item tests per second" is measured |
| 7 | **Smart Object claims give exclusivity, not arbitration.** A slot is claimable if free, or claimed at a lower priority; the first `MarkSlotAsClaimed` wins (`SO/Public/SmartObjectRuntime.h:160-166`; `SO/Private/SmartObjectSubsystem.cpp:1383-1438`) ✔ | Claims are the engine's own answer to "two agents pick the same cover", so they **must be a baseline** (B1, B2). What they leave open is *who* should get which slot. That is the part SquadQuery adds (joint assignment) |
| 8 | **The EQS Smart Object generator lists only slots claimable at generation time** (`bOnlyClaimable = true`, `SO/Public/EnvQueryGenerator_SmartObjects.h:38-39`; `.cpp:77`). The claim happens after the query finishes, possibly frames later ✔ | A race window **[inferred]**: claims can fail, forcing another query. Counted as "claim failures". An agent never sees its own claimed slot in its own results, so the baseline must compare a new pick with its current slot (otherwise it would thrash and be a strawman) |
| 9 | **Runtime navmesh needs opting in.** `BaseEngine.ini` sets `RuntimeGeneration=Static` (`:3042-3043`); `ERuntimeGenerationType::Dynamic` supports geometry changes (`NS/Public/NavigationData.h:523-533`). Bounds can come from an `ANavMeshBoundsVolume` spawned at runtime: `OnNavigationBoundsUpdated` is public and reads `GetComponentsBoundingBox(true)`; without the editor, a brush's bounds come from `BrushBodySetup->AggGeom` (`NS/Private/NavigationSystem.cpp:4136-4161`; `Engine/Private/Components/BrushComponent.cpp:459-486`) ✔ | `DefaultEngine.ini` sets `RuntimeGeneration=Dynamic` for `RecastNavMesh`. The arena spawns one bounds volume with a box body, then waits for `IsNavigationBuildInProgress()` to be false before agents spawn, so navmesh building never falls inside the measured window. The config half is **verified** (Phase 4: the `RecastNavMesh` default object reads `Dynamic`, uncooked and packaged, `09` R-4, R-7); building a navmesh from a runtime volume is **[to verify in S1]**, including in a cooked build. Navigation invokers were rejected: they rebuild tiles around moving agents during the run |
| 10 | **The Smart Object generator accepts several origins and removes duplicates** (`SO/Private/EnvQueryGenerator_SmartObjects.cpp:45-75`) ✔ | The squad query's origin context returns **every member's location**. The squad's candidate set is then exactly the union of the members' per-agent candidate sets, with the same box size. The comparison is fair by construction, and the squad set is no larger than needed (#3) |
| 11 | **Smart Objects can be built without assets**: Epic's own tests use `NewObject<USmartObjectDefinition>` + `DebugAddSlot()` (not editor-only), `SetDefinition`, then registration; game-world components register in `BeginPlay` (`SO/Public/SmartObjectDefinition.h:326-327`; `SmartObjectsTestSuite/Private/SmartObjectsTest.cpp:65-72, 109-110`; `SO/Private/SmartObjectComponent.cpp:173-185, 296-300, 359-372`) ✔ | Cover slots are Smart Object slots on runtime-spawned cover blocks, created from one runtime definition. No binary assets. **[to verify in S1]** in a cooked build; fallback: create the definition asset with an editor Python script through the queue |
| 12 | **A 5.8 StateTree can sleep until an event.** `SendStateTreeEvent` queues an event (max 64; extras are dropped with an error) and schedules a tick; `UStateTreeComponent` allows scheduled ticking by default and disables its tick when told to sleep (`GST/Public/Components/StateTreeComponent.h:116-120`; `ST/Public/StateTreeEvents.h:181`; `ST/Private/StateTreeEvents.cpp:45-49`; `ST/Private/StateTreeExecutionContext.cpp:918-938`; `GST/Private/Components/StateTreeComponent.cpp:17-25, 298-330`) ✔ | Event-driven re-evaluation fits StateTree natively, and the old claim "StateTree ticks every frame by default" ([research/02](Research/02_technical_challenges.md) [T9-C], a 5.7 thread) **is not a fixed rule in 5.8**. The system must send one event per assignment change, never one per frame (64-event cap). A StateTree's runtime data is compiled from editor-only `EditorData` (`ST/Public/StateTree.h:381-384`) [inferred: no runtime authoring], so StateTree enters as an adapter step with an authored asset (§8) |
| 13 | **The idea is not new.** A 2014 UE4 forum design: a "squad helper object … scores each node and shares this information", with "the full score … calculated by each AI based on weightage". F.E.A.R. (2005): the squad behaviour selects a cover node and "ensure[s] that other A.I. are not ordered to go to the same node". Killzone 3: the squad constrains individual position picking "thus improving performance". CoverPoints (UE 5.8, Fab/GitHub): per-point claim/release and trace-free scoring. SquadAI (GitHub): squad "cover allocation", method not described. **None publishes measurements** | **Repositioned.** SquadQuery is presented as a measured, engine-native implementation of a known idea: built on 5.8's EQS and Smart Object claims, with optimal (minimum total cost) joint assignment, compared with per-agent EQS, claim-based and event-driven baselines on cost and decision quality. Credit is given explicitly (§5) |
| 14 | **Sight is already time-sliced**: `MaxTracesPerTick = 6`, `MaxAsyncTracesPerTick = 10`, `MaxTimeSlicePerTick = 0.005`, checked every 40 queries (`AIModule/Private/Perception/AISense_Sight.cpp:46-51, 134-145, 295-297, 327-333`) ✔ | The research claim is **confirmed**. If agents detected the threat by sight, time-to-cover would include perception latency that grows with N in every configuration. v1 delivers the threat as a scripted squad event; perception stays out of the measured loop (`01` §8) |
| 15 | **Changing a destination costs a synchronous path query**: `MoveToLocation` → `FindPathForMoveRequest` → `FindPathSync` on the game thread (`AIModule/Private/AIController.cpp:612, 899-906`) ✔ | Decision switches cost CPU, not just quality. `MoveToLocation` calls are counted and their cost sits inside the AI time |
| 16 | **The engine's per-agent polling default is 0.5 s ± 0.1 s** (`AIModule/Private/BehaviorTree/BTService.cpp:20-21`; the "Run EQS query on a regular basis" service, `BTService_RunEQS.cpp:20`) ✔ | The polled baselines B0 and B1 re-query every 0.5 s with a seeded ±0.1 s deviation: what a Behavior Tree would do by default |
| 17 | **A query can be built in C++ at runtime with public API**: `UEnvQuery::GetOptionsMutable()`, public option and test members, `QueryName` set in `PostInitProperties` (`AIModule/Classes/EnvironmentQuery/EnvQuery.h:66`; `AIModule/Private/EnvironmentQuery/EnvQuery.cpp:156-160`) ✔. The Smart Object generator's settings are `protected` (`SO/Public/EnvQueryGenerator_SmartObjects.h:19-39`) | The benchmark's two queries are built in code, identical across configurations and visible in review. A tiny generator subclass sets the protected settings. A shipping game would author the same queries as assets, and the system accepts any `UEnvQuery` |
| 18 | *(added in step S1)* **A Smart Object request filter excludes claimed slots by default** (`bShouldIncludeClaimedSlots = false`, `SO/Public/SmartObjectRequestTypes.h:111-112`; applied at `SO/Private/SmartObjectSubsystem.cpp:2696-2699`), and **the query box tests the smart object's own location, not each slot's** (`:2880`) ✔ | The shared squad query must set `bShouldIncludeClaimedSlots = true` as well as `bOnlyClaimable = false`, or the squad would never see the slots its own members hold (02 §7). A box around a member includes every slot of every block whose centre is inside it, so slots can lie up to 2.1 m outside the box. The per-agent and squad boxes have the same half-extent, so this is the same in every configuration |
| 19 | *(added in step S1)* **A Smart Object definition is valid only if every slot has a behavior definition** (or the definition has a default one), and a component with an invalid definition never registers (`SO/Private/SmartObjectDefinition.cpp:205-222`; `SO/Private/SmartObjectSubsystem.cpp:749-770`). The definition is validated lazily at registration (`:749-754`) ✔ | The game module adds a one-line behavior class, `USQCoverBehaviorDefinition`, the same shape as Epic's test type. Each runtime slot gets one instance |
| 20 | *(added in step S1, found by the S1 self-check)* **A component's bounds are cached, and the navigation system reads the cached value.** A bounds volume spawned at runtime registers during the spawn (`Engine/Private/Actor.cpp:4330-4336`, also for a deferred spawn), before it has a box body, so its cached bounds are a point; `OnNavigationBoundsUpdated` takes `GetComponentsBoundingBox(true)` (`NS/Private/NavigationSystem.cpp:4145`), which sums the cached `Bounds` (`Engine/Private/Actor.cpp:2272-2274`), and the update is skipped as empty bounds. `UpdateBounds()` after assigning the body fixes it. **The build-finished event** (`UNavigationSystemV1::OnNavigationGenerationFinishedDelegate`, broadcast whenever the Recast generator's task count drops to zero: `NS/Private/NavMesh/RecastNavMeshGenerator.cpp:7614-7631` → `RecastNavMesh.cpp:3237-3240` → `NavigationSystem.cpp:4913-4915`) **also fires for such a near-empty build** ✔ (observed in `09` S1-c) | The arena calls `UpdateBounds()` before announcing the volume, and readiness is the event **plus** a corner-to-corner path test (`05` B5). Corrects #9's recipe, which assumed the box body is read when the volume is announced |
| 21 | *(added in step S1, from the self-check)* **The editor and the game binary use different floating-point modes.** With `BuildSettingsVersion.V7`, UBT compiles Editor and Program targets with `/fp:precise` and Game, Client and Server targets with `/fp:fast` (`Programs/UnrealBuildTool/Platform/Windows/VCToolChain.cs:1324-1335`; the V7 note in `Configuration/Rules/TargetRules.cs:3737-3740`) ✔ | The same seed gives a bit-identical arena within one binary, but a different hash in the editor (`-game`) than in the packaged build, with identical layout statistics (`09` S1, observation 1). Hashes and decisions are compared only between runs of one binary; every measured run uses the packaged build |

**Benchmark-plan changes:**

| # | Finding | Consequence |
|---|---|---|
| B-1 | EQS's time slice uses wall-clock time (`EnvQueryInstance.cpp:590`; `EnvQueryTypes.h:1221`) **[inferred]** | Even fixed-step runs aren't bit-identical in *when* a query finishes, so decisions can differ between runs. Decision metrics are reported as distributions across runs, never from one run |
| B-2 | The CPU is hybrid; Windows sometimes ran UE5-UpdateBudget's game thread on an E-core (runs about 1.6× slower) | Every timed run is pinned to the performance cores: the run script pins itself once (`pin_self`, `SetProcessAffinityMask` on its own process) before starting anything, so each game inherits the mask from its first instruction; the real game executable is launched directly, never through `cmd /c start /affinity`, which always returns exit code 0 and would hide a crash. The mask read back (`applied_mask`) is recorded |
| B-3 | Fixed step, CSV capture on an event, `-traceautostart=0` and the CSV columns were re-opened on 5.8.4 (`LaunchEngineLoop.cpp:2457-2462`; `UnrealEngine.cpp:3015-3023`; `CsvProfiler.cpp:73, 4309, 5078-5081`) ✔ | The UE5-UpdateBudget protocol is reused unchanged: packaged Development, `-deterministic -FPS=60`, `BenchStart`/`BenchEnd` events |

---

## 2. EQS (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| `UEnvQueryManager : UAISubsystem`; `UAISubsystem : UObject, FTickableGameObject` | `AIModule/Classes/EnvironmentQuery/EnvQueryManager.h:207`; `AIModule/Classes/AISubsystem.h:15` ✔ |
| `RunQuery` appends to `RunningQueries`; `PrepareQueryInstance` + `RunQuery(TSharedPtr<FEnvQueryInstance>)` let the caller keep the instance | `AIModule/Private/EnvironmentQuery/EnvQueryManager.cpp:287-303, 377-404`; `EnvQueryManager.h:224, 242` ✔ |
| `RunInstantQuery` bypasses "all EQS perf controlling and time slicing mechanics" | `EnvQueryManager.h:235-239` ✔ |
| Global slice, breadth first, wrap-around; time slicing on by default | `EnvQueryManager.cpp:446, 456-460, 482, 550-553, 567-570, 573-577`; `:57` ✔ |
| One step = the generator, or one test up to a deadline; a test can resume next frame | `AIModule/Private/EnvironmentQuery/EnvQueryInstance.cpp:292-521, 590`; `AIModule/Classes/EnvironmentQuery/EnvQueryTypes.h:1221` ✔ |
| Tests sorted: filters first, then by cost; trace = High, distance = Low; auto-sort on by default | `EnvQueryManager.cpp:741-753, 945`; `Tests/EnvQueryTest_Trace.cpp:112`; `Tests/EnvQueryTest_Distance.cpp:55`; `EnvQueryGenerator.cpp:9` ✔ |
| Single-result shortcut, only for a test that can be a final condition | `EnvQueryTest.h:156-161` ✔ |
| Discarded items are skipped by later tests | `EnvQueryTypes.h:1075-1080` ✔ |
| The trace test traces once per item per context location, synchronously | `Tests/EnvQueryTest_Trace.cpp:93-104` ✔ |
| Run modes: `SingleResult`, `RandomBest5Pct`, `RandomBest25Pct`, `AllMatching` | `EnvQueryTypes.h:183-192` ✔ |
| Contexts are evaluated once per instance and cached in that instance | `EnvQueryTypes.h:811-813`; `EnvQueryInstance.cpp:117-137` |
| Mass EQS runs EQS work for Mass entities in processors; still per query instance | `Plugins/Runtime/MassGameplay/Source/MassEQS/Public/MassEQSSubsystem.h` (class comment and `PushRequest`) |

## 3. Smart Objects, StateTree, perception (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| SmartObjects plugin is off by default | `Plugins/Runtime/SmartObjects/SmartObjects.uplugin:13` ✔ |
| Slot states Free / Claimed / Occupied | `SO/Public/SmartObjectRuntime.h:22-32` ✔ |
| Priorities Low … High; a higher priority aborts a lower claim and fires its invalidation callback | `SO/Public/SmartObjectTypes.h:80-92`; `SO/Private/SmartObjectSubsystem.cpp:1404-1418`; `SO/Private/SmartObjectRuntime.cpp:146-181` ✔ |
| Release with `MarkSlotAsFree`; the old `Release` is in the deprecated block | `SO/Public/SmartObjectSubsystem.h:684, 1474-1487` ✔ |
| Slot-invalidation callback registration | `SO/Public/SmartObjectSubsystem.h:963, 969` |
| The subsystem is not thread-safe by default | `SO/Public/SmartObjectSubsystem.h:273-295` |
| EQS Smart Object generator: claimable-only by default; multiple origins de-duplicated; vector-based item type | `SO/Public/EnvQueryGenerator_SmartObjects.h:10-39`; `SO/Private/EnvQueryGenerator_SmartObjects.cpp:45-84`; `SO/Public/EnvQueryItemType_SmartObject.h:31` ✔ |
| StateTree event queue max 64; drop with error; event wakes the tree | `ST/Public/StateTreeEvents.h:176-181`; `ST/Private/StateTreeEvents.cpp:45-49`; `ST/Private/StateTreeExecutionContext.cpp:918-938` ✔ |
| StateTree component: scheduled tick allowed by default (two cvars), tick disabled on sleep | `GST/Private/Components/StateTreeComponent.cpp:17-25, 298-330`; `GST/Private/Components/StateTreeComponentSchema.cpp:24-28, 58-70` ✔ |
| StateTree "Run Env Query" task: per-agent owner, `SingleResult` by default | `GST/Public/Tasks/StateTreeRunEnvQueryTask.h:24, 36, 49` ✔ |
| Sight defaults and time-slice checks | `AIModule/Private/Perception/AISense_Sight.cpp:46-51, 134-145, 295-297, 327-333`; `AIModule/Classes/Perception/AISense_Sight.h:203, 244-277` ✔ |
| Team sense broadcasts stimuli to teammates | `AIModule/Classes/Perception/AISense_Team.h:14, 50` |
| Async line trace API (not used in v1) | `Source/Runtime/Engine/Classes/Engine/World.h:2522, 2708` |

`GST` = `Plugins/Runtime/GameplayStateTree/Source/GameplayStateTreeModule`, `ST` = `Plugins/Runtime/StateTree/Source/StateTreeModule`, `NS` = `Source/Runtime/NavigationSystem`.

## 4. Navigation and measurement (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| Runtime generation types; `RuntimeGeneration` is an `ANavigationData` config property (`config=Engine`); BaseEngine sets Static | `NS/Public/NavigationData.h:523-533, 539, 581-583`; `Config/BaseEngine.ini:3042-3043` ✔ |
| 5.8: runtime-spawned nav data gets one initial rebuild even when Static | `NS/Public/NavigationData.h:584-590` |
| Nav data is auto-spawned when bounds appear (`bAutoCreateNavigationData=true`) | `Config/BaseEngine.ini:3038-3039`; `NS/Private/NavigationSystem.cpp:4380-4390` ✔ |
| Spawned bounds volumes announce themselves; `OnNavigationBoundsUpdated` is public; `AddNavigationBounds` is protected | `NS/Private/NavMesh/NavMeshBoundsVolume.cpp:117-127`; `NS/Public/NavigationSystem.h:854, 879, 1094-1096` ✔ |
| Brush bounds without the editor come from `BrushBodySetup->AggGeom`; `BrushBodySetup` is public | `Engine/Private/Components/BrushComponent.cpp:459-486`; `Engine/Classes/Components/BrushComponent.h:28-30` ✔ |
| Build-in-progress query | `NS/Public/NavigationSystem.h:1100-1101` ✔ |
| Pawns ignore the Visibility channel by default, so agents don't block cover traces | `Config/BaseEngine.ini:3110` (profile `Pawn`: `CustomResponses=((Channel="Visibility",Response=ECR_Ignore))`); cover blocks use `BlockAll` (`:3104`) ✔ |
| Measurement facts (fixed step, CSV events, exclusive category, auto-trace) | `Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:2457-2462`; `Source/Runtime/Engine/Private/UnrealEngine.cpp:3015-3023`; `Source/Runtime/Core/Private/ProfilingDebugging/CsvProfiler.cpp:73, 4309, 5078-5081`; `Source/Runtime/Core/Public/ProfilingDebugging/CsvProfiler.h:129-131`; `Source/Runtime/Core/Private/ProfilingDebugging/TraceAuxiliary.cpp:267, 2378` ✔ |

## 5. Existing work, updated

| Solution | Shares evaluation? | Prevents conflicts? | Joint (optimal) assignment? | Event-driven? | Published measurements | Evidence |
|---|---|---|---|---|---|---|
| **EQS per agent** (BT service, StateTree task) | No | No | No | BT: polled 0.5 s; StateTree: on state entry | — | source ✔ |
| **Smart Object claims** | No | **Yes** (exclusive slots, priorities) | No (first come, first served) | n/a | — | source ✔, 5.8 docs opened |
| **CoverPoints** (UE 5.8, source-available, paid) | No (per-agent find) | **Yes** (claim/release with expiry) | No | Not described | None ("same … whether … two hundred points or twenty thousand", no conditions) | README opened |
| Tactical Combat AI Framework (Fab) | No | Not mentioned | No | Not described | None | listing opened |
| SquadAI (GitHub) | Not described | "prevent every AI agent from simply choosing the nearest available cover" | Not described | Not described | None | README opened |
| Squad & Team Combat AI (Marketplace, 4.26–5.4) | Squad leader orders | Not described | Not described | Not described | None | snippet only |
| **Frozenfire, UE4 forum (2014)** | **Yes** (squad helper scores nodes and shares them; per-AI weights) | Not described | No | Not described | None | thread opened |
| **F.E.A.R. (Orkin, GDC 2006)** | No (each AI's sensors) | **Yes** ("ensure that other A.I. are not ordered to go to the same node") | Greedy per behaviour | Squad behaviours activate on situation changes | None | paper opened |
| Killzone 3 (Game AI Pro 2013, ch. 29) | Squad corridor constrains individual picking | Distance from friendlies as a score | No | Planner-driven | Qualitative ("improving performance") | chapter opened |

**The gap, stated conservatively:**
- Shared squad evaluation (2014) and squad-level conflict-free selection (2005) **already exist**. So do per-point reservations (engine, CoverPoints) and trace-free cover scoring (CoverPoints).
- Among the sources reviewed, none (a) builds the shared evaluation **on UE 5.8's own EQS and Smart Object claims**, (b) uses an **optimal** joint assignment and compares it with greedy claiming, or (c) publishes **controlled measurements of cost and decision quality** against per-agent EQS baselines at several agent counts.
- Gap strength: **weak for the idea, medium for the measured, engine-native study.** The roadmap's "Medium (re-checked in Phase 2)" is lowered for the idea and kept for the study. Studio-internal systems aren't public, and a web search can't prove absence.

## 6. Hypothesis status after Phase 2

| # | Hypothesis | Status |
|---|---|---|
| H1 | Per-agent queries repeat agent-independent work (visibility from the threat, distance to the threat) | **Supported by code** (no result sharing, #4) **[to measure]** |
| **H2 (new)** | The single-result shortcut caps per-agent trace work, so sharing may not reduce traces against an event-driven per-agent baseline | **Supported by code** (#3) **[to measure]**. This is the main risk to SC1/SC3 |
| H3 | Polling repeats work when nothing changed | Unchanged **[to measure]** (B0/B1 vs B2) |
| H4 | Independent choices collide | Unchanged **[to measure]** (B0) |
| H5 | Claims remove collisions but cost retries and give a worse joint outcome | **Supported by code** (#7, #8: race window, first come first served) **[to measure]** |
| H6 | Event-driven work bunches into event frames (spikes) | Unchanged **[to measure]** |
| H7 | The global EQS slice adds queueing latency at high query counts | **Supported by code** (#1, #2) **[to measure]** |

## 7. Carried into Phase 3

- Cover slots are Smart Object slots on runtime-spawned cover blocks; one runtime definition. **[to verify in S1]**
- One cover-slot generator for every configuration: a subclass of the engine's Smart Object generator, so the item source is identical. Per-agent queries use the querier as origin; the squad query uses all member locations (#10).
- Per-agent queries are `SingleResult` (the engine's shortcut stays on: the baseline must not be a strawman); the squad query is `AllMatching`.
- AI time = `Exclusive/GameThread/EnvQueryManager` + one exclusive stat around all our decision code. Item tests from EQS debug data (Development only).
- `RuntimeGeneration=Dynamic`; a runtime bounds volume with a box body; wait for the build before agents spawn. **[to verify in S1]**
- Threat delivered as a scripted squad event; no perception in the measured loop.
- Every timed run pinned to the performance cores; mask recorded.

## 8. Decisions taken in Phase 2 (owner's standing rule: best for a credible portfolio, reason recorded)

| Decision | Reason |
|---|---|
| Keep the project and narrow it (no swap) | The idea exists (#13), but nothing reviewed measures it. A careful measured study, with the engine's own tools as baselines, is a stronger portfolio piece than a claim of novelty |
| Use real Smart Object claims in the baselines and in SquadQuery | They are the engine's existing answer to conflicts. Replacing them with a home-made reservation map would make the comparison weaker |
| StateTree enters as an adapter (S8), not as the measured brain | A StateTree needs an editor-authored asset (#12). Putting it in the measured loop would add an asset pipeline and equal cost to every configuration without changing the comparison. The adapter sends one StateTree event per assignment change; its asset is created with an editor Python script through the queue, or by hand if scripting can't build it, and the result is recorded in `09` |
| Perception out of the measured loop | Its latency grows with N in every configuration and would hide decision latency (#14) |
