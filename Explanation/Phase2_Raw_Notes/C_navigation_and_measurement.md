> **Raw Phase 2 research notes (C_navigation_and_measurement), UE 5.8.4, 2026-11-21.** Working notes kept as an evidence trail. Every claim cites a `File:line` or a URL. **The claims the design depends on were re-checked by hand and are restated in [../01b_Research_Findings.md](../01b_Research_Findings.md) (marked ✔). Treat anything here that is not restated there as unverified.**

# P07 SquadQuery: Phase 2 research, part C: a navigable arena built at runtime, and how to measure it (UE 5.8.4)

Paths are relative to `Engine/`. `NS` means `Source/Runtime/NavigationSystem`.

---

## C1. The problem

The project has no hand-authored map (the same choice as UE5-UpdateBudget: everything is spawned by code from a fixed seed, so every run is identical and reviewable). Agents need a **navmesh** to move with `AAIController::MoveToLocation`, and the navmesh must cover geometry that only exists after the game starts. Three questions:
1. Will the engine build a navmesh at runtime at all?
2. Where do its **bounds** come from, when no `ANavMeshBoundsVolume` was placed in a map?
3. How does the game know the navmesh is ready before agents start?

## C2. Runtime generation is off by default

| Fact | Source |
|---|---|
| `ERuntimeGenerationType`: `Static`, `DynamicModifiersOnly`, `Dynamic` ("Fully dynamic, supports geometry changes along with navigation modifiers"), `LegacyGeneration` | `NS/Public/NavigationData.h:523-533` |
| The setting is a config property of `ANavigationData` (`config=Engine`) | `NS/Public/NavigationData.h:539, 581-583` |
| **`BaseEngine.ini` sets `RuntimeGeneration=Static`** | `Config/BaseEngine.ini:3042-3043` |
| `ARecastNavMesh` is also `config=Engine`, so a `[/Script/NavigationSystem.RecastNavMesh]` section in the project's `DefaultEngine.ini` overrides the parent's value for the Recast class | `NS/Public/NavMesh/RecastNavMesh.h:570` **[inferred]** from the config inheritance rule; checked at run time in step S1 |
| New in 5.8: nav data spawned at runtime because none was serialized is marked `bRequiresInitialRebuild`, allowing **one** rebuild even when Static | `NS/Public/NavigationData.h:584-590, 659-661, 680` |
| `bAutoCreateNavigationData=true` (BaseEngine.ini): the navigation system spawns the `ARecastNavMesh` itself once bounds exist | `Config/BaseEngine.ini:3038-3039`; `NS/Private/NavigationSystem.cpp:4380-4390` (`SpawnMissingNavigationData()` in `PerformNavigationBoundsUpdate`) |

**Consequence:** the project sets `RuntimeGeneration=Dynamic` for `RecastNavMesh`. The one-time rebuild of a Static navmesh would also cover geometry spawned *before* the navmesh, but it isn't documented as a contract for geometry spawned later, so we don't rely on it.

## C3. Navigation bounds without a placed volume

| Fact | Source |
|---|---|
| At startup the system gathers bounds from every `ANavMeshBoundsVolume` in the world | `NS/Private/NavigationSystem.cpp:4400-4418` (`TActorIterator<ANavMeshBoundsVolume>`) |
| A volume spawned later announces itself in `PostRegisterAllComponents` → `OnNavigationBoundsAdded(this)` | `NS/Private/NavMesh/NavMeshBoundsVolume.cpp:117-127` |
| `OnNavigationBoundsUpdated(ANavMeshBoundsVolume*)` is **public** and Blueprint-callable. It reads the volume's `GetComponentsBoundingBox(true)`; an invalid box becomes a removal | `NS/Public/NavigationSystem.h:1094-1096`; `NS/Private/NavigationSystem.cpp:4136-4161` |
| `AddNavigationBounds(const FNavigationBounds&)` exists but is **protected** | `NS/Public/NavigationSystem.h:854, 879` |
| Bounds changes are ignored when the navigation system is static (`IsNavigationSystemStatic()`, i.e. `bStaticRuntimeNavigation`, default false) | `NS/Private/NavigationSystem.cpp:4138, 744`; `NS/Public/NavigationSystem.h:1201-1208` |
| A volume's bounds come from its brush component. **Without the editor**, `UBrushComponent::CalcBounds` uses `BrushBodySetup->AggGeom` if it has elements, otherwise a zero-size box at the actor location | `Source/Runtime/Engine/Private/Components/BrushComponent.cpp:459-486` (the editor-only `Brush->Polys` branch is under `#if WITH_EDITOR` at `:461-475`) |
| `BrushBodySetup` is a public `UPROPERTY` | `Source/Runtime/Engine/Classes/Components/BrushComponent.h:28-30` |

**Plan [inferred], to verify in step S1 (the first runtime check):**
1. Spawn the floor and cover blocks (static mesh components with collision; they affect navigation by default).
2. Spawn one `ANavMeshBoundsVolume` at the arena centre. Give its brush component a `UBodySetup` with one box element the size of the arena, update its bounds, and call `NavSys->OnNavigationBoundsUpdated(Volume)`.
3. Wait until `UNavigationSystemV1::IsNavigationBuildInProgress()` is false (`NS/Public/NavigationSystem.h:1100-1101`). Only then spawn agents and start the warm-up.

**Rejected: navigation invokers** (`bGenerateNavigationOnlyAroundNavigationInvokers`, `NS/Public/NavigationSystem.h:383-401`). Invokers generate and remove tiles around moving agents all the time. That would put navmesh building inside the measured window, and its cost would differ between configurations that move agents differently. Bounds plus one build before warm-up keep navigation out of the measurement.

**Fallback** if step S1 shows the runtime volume isn't picked up in a cooked build: an editor Python script, run through the queue, saves a map containing only a `NavMeshBoundsVolume` sized to the arena. Geometry stays runtime-built and seeded.

## C4. Measurement facts reused from UE5-UpdateBudget (re-opened on 5.8.4)

| Fact | Source |
|---|---|
| `-deterministic` = `-UseFixedTimeStep -FixedSeed` | `Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:2457-2462` |
| A fixed step sets every frame's delta to the fixed value and does not wait | `Source/Runtime/Engine/Private/UnrealEngine.cpp:3015-3023` |
| Capture on an event: `-csvStartOnEvent=`, `-csvCaptureOnEventFrameCount=`; exit with `-ExitAfterCsvProfiling` | `Source/Runtime/Core/Private/ProfilingDebugging/CsvProfiler.cpp:5078-5081, 4309` |
| The CSV category `Exclusive` is enabled by default; `CSV_SCOPED_TIMING_STAT_EXCLUSIVE` records into it | `CsvProfiler.cpp:73`; `Source/Runtime/Core/Public/ProfilingDebugging/CsvProfiler.h:129-131` |
| An open Unreal Insights auto-traces Development runs unless `-traceautostart=0` | `Source/Runtime/Core/Private/ProfilingDebugging/TraceAuxiliary.cpp:267, 2378` |
| Columns present in a packaged Development capture on this machine include `Exclusive/GameThread/EnvQueryManager`, `Exclusive/GameThread/AIPerception`, `Exclusive/GameThread/NavigationBuild`, `Exclusive/GameThread/Tickables` | header of UE5-UpdateBudget batch `2026-11-21_001816_s3smoke2`, run `B0_N1000_r1` |
| `GameThreadTime` is 0 under `-nullrhi`; headline runs need the real RHI | UE5-UpdateBudget `09`, step S1 finding 1 |
| The launcher engine can't build the Test configuration; headline builds are packaged Development | UE5-UpdateBudget `09`, F-3a |
| **This CPU is hybrid** (i7-14650HX: P-cores are logical 0–15, mask `0xFFFF`). UE5-UpdateBudget's baseline batch showed runs at two stable levels about 1.6× apart, with Windows placing the game thread on an E-core. The run script pins **itself** once with `SetProcessAffinityMask` on its own process (`pin_self`) before it starts anything; every game it launches then inherits the mask from its first instruction. It launches the real game executable (`Build\Windows\<Name>\Binaries\Win64\<Name>.exe`, not the bootstrap) with `subprocess.Popen`, a timeout and a kill in `finally`. The mask comes from `GetLogicalProcessorInformationEx`; `applied_mask` reads back what Windows applied, for the environment record. Launching through `cmd /c start /affinity` was rejected: it always returns exit code 0, so failures go undetected | UE5-UpdateBudget `Tools/run_bench.py` (`performance_core_mask`, `pin_self`, `applied_mask`, direct launch); portfolio measurement protocol (2026-11-21, revised twice the same day) |

## C5. Which time counts as "AI time"

What the CSV gives us, and what we add:

| Work | Where it runs | How it is measured |
|---|---|---|
| EQS generators and tests, all configurations | `UEnvQueryManager::Tick` (tickables) | engine column `Exclusive/GameThread/EnvQueryManager` |
| Our code inside EQS finish callbacks (pick, claim, assignment) | inside the EQS tick, but its time is excluded from the EQS budget (part A, A2) | our own exclusive stat, so it is **subtracted** from `EnvQueryManager` and not counted twice **[inferred]** from how exclusive CSV stats nest; checked in step S2 |
| Our decision code outside EQS (polling, event handling, `MoveToLocation` with its synchronous path query) | our components and subsystems | the same exclusive stat |
| Character movement and path following | engine, identical work per moving agent | not AI decision time; reported through `GameThreadTime` |

**AI ms per frame** = `Exclusive/GameThread/EnvQueryManager` + our exclusive decision stat. The definition is fixed in `01` §6.3.
