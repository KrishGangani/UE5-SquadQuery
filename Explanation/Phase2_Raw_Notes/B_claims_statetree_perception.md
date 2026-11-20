> **Raw Phase 2 research notes (B_claims_statetree_perception), UE 5.8.4, 2026-11-21.** Working notes kept as an evidence trail. Every claim cites a `File:line` or a URL. **The claims the design depends on were re-checked by hand and are restated in [../01b_Research_Findings.md](../01b_Research_Findings.md) (marked ✔). Treat anything here that is not restated there as unverified.**

# P07 SquadQuery: Phase 2 research, part B: Smart Object claims, StateTree events, AI Perception (UE 5.8.4)

Paths are relative to `Engine/`. `SO` means `Plugins/Runtime/SmartObjects/Source/SmartObjectsModule`. `GST` means `Plugins/Runtime/GameplayStateTree/Source/GameplayStateTreeModule`. `ST` means `Plugins/Runtime/StateTree/Source/StateTreeModule`.

---

## B1. Smart Object claims: what they guarantee

Epic's 5.8 overview (page title "Smart Objects in Unreal Engine - Overview | Unreal Engine 5.8 Documentation", https://dev.epicgames.com/documentation/en-us/unreal-engine/smart-objects-in-unreal-engine---overview?application_version=5.8, **opened**): Smart Objects "represent a set of activities in the level that can be used through a reservation system". "The claimed Slot may not be claimed by another Agent until it is released by the Agent that claimed it." "Agents are responsible for releasing their claimed Slots."

| Fact | Source |
|---|---|
| The plugin is **off by default**; a project must enable it | `Plugins/Runtime/SmartObjects/SmartObjects.uplugin:13` `"EnabledByDefault" : false` |
| The subsystem is a `UWorldSubsystem` and "offers the API for spatial queries and reservations". It is **not thread-safe** by default | `SO/Public/SmartObjectSubsystem.h:273-295` |
| Slot states: `Free`, `Claimed` ("claimed but interaction is not active yet"), `Occupied` | `SO/Public/SmartObjectRuntime.h:22-32` |
| A slot can be claimed if it is enabled **and** either free, or claimed at a **lower** priority than the new claim | `SO/Public/SmartObjectRuntime.h:160-166` |
| Claim priorities: `Low`, `BelowNormal`, `Normal`, `AboveNormal`, `High` | `SO/Public/SmartObjectTypes.h:80-92` |
| `MarkSlotAsClaimed(SlotHandle, ClaimPriority, UserData)` returns a claim handle, or an invalid handle if the slot can't be claimed | `SO/Public/SmartObjectSubsystem.h:631`; `SO/Private/SmartObjectSubsystem.cpp:1383-1438` |
| A higher-priority claim **releases the existing claim as aborted** and fires the slot-changed notification before claiming | `SO/Private/SmartObjectSubsystem.cpp:1404-1418` |
| An aborted release calls the holder's "slot invalidated" callback, if one was registered | `SO/Private/SmartObjectRuntime.cpp:146-181` (callback at `:169`); registration API `SO/Public/SmartObjectSubsystem.h:963, 969` |
| Release is `MarkSlotAsFree(ClaimHandle)`. The older `Release` sits in the header's deprecated block, so new code calls `MarkSlotAsFree`. Only the claiming user's handle can release a slot | `SO/Public/SmartObjectSubsystem.h:684, 1474-1487`; `SO/Private/SmartObjectRuntime.cpp:155-164` |
| `CanBeClaimed(SlotHandle, Priority = Normal)` asks without claiming | `SO/Public/SmartObjectSubsystem.h:622`; `SO/Private/SmartObjectSubsystem.cpp:1441-1451` |

**What claims solve, and what they don't.**
- They solve **exclusivity**: two agents can't hold the same slot at the same priority.
- They don't decide **who should get which slot**. The first agent to call `MarkSlotAsClaimed` wins. **[inferred]** from the code: there is no arbitration between simultaneous requests other than call order and priority.
- They don't coordinate the search. Each agent still runs its own query.

## B2. Smart Objects inside EQS

`SO/Public/EnvQueryGenerator_SmartObjects.h` and `SO/Private/EnvQueryGenerator_SmartObjects.cpp`:

| Fact | Source |
|---|---|
| A built-in EQS generator, "Fetches Smart Object slots within QueryBoxExtent from locations given by QueryOriginContext, that match SmartObjectRequestFilter" | `EnvQueryGenerator_SmartObjects.h:10-12` |
| It calls `FindSmartObjects` for a box around each origin | `EnvQueryGenerator_SmartObjects.cpp:45-53` |
| **By default it returns only slots that can be claimed right now** (`bOnlyClaimable = true`) | `EnvQueryGenerator_SmartObjects.h:38-39`; `.cpp:77` |
| Items are `UEnvQueryItemType_SmartObject`, which derive from the vector item type, so location tests (trace, distance) work on them | `SO/Public/EnvQueryItemType_SmartObject.h:31`; `.cpp:84` (`AddItemData<UEnvQueryItemType_SmartObject>`) |
| Its settings are `protected` `UPROPERTY`s | `EnvQueryGenerator_SmartObjects.h:19-39` |

**Race window [inferred].** "Claimable" is checked when items are **generated**. The claim happens later, after the query has finished, which may be frames later under time-slicing (part A, A2). Another agent can claim the slot in between, and `MarkSlotAsClaimed` then fails. The agent must query again. Two side effects for the per-agent baseline:
1. Claim failures cost a full extra query.
2. With `bOnlyClaimable`, an agent's **own** claimed slot is not claimable by itself (`CanBeClaimed` with the same priority is false, `SmartObjectRuntime.h:163-165`). A polling agent never sees its current slot in its own results. Unless the agent code compares the new best with its current slot, every poll picks a different slot. The baseline must include that comparison so it isn't a strawman.

## B3. Building Smart Objects at runtime (no assets)

| Fact | Source |
|---|---|
| `USmartObjectDefinition` is a data asset, but `DebugAddSlot()` adds a slot from code ("used for testing purposes"). It is not inside `#if WITH_EDITOR` | `SO/Public/SmartObjectDefinition.h:258, 326-327` |
| Epic's own Smart Object tests build a definition with `NewObject` + `DebugAddSlot()`, give it to components with `SetDefinition`, and register them | `Plugins/Runtime/SmartObjects/Source/SmartObjectsTestSuite/Private/SmartObjectsTest.cpp:65-72, 109-110` |
| In game worlds a `USmartObjectComponent` registers itself with the subsystem in `BeginPlay`; `SetDefinition` must come before registration | `SO/Private/SmartObjectComponent.cpp:173-185` (game worlds wait for `BeginPlay`), `:296-300` (registration in `BeginPlay`), `:227-250`, `:359-372` (`SetDefinition` refuses once registered) |
| An external system can also create a runtime instance with `CreateSmartObject(Definition, Transform, OwnerData)` | `SO/Public/SmartObjectSubsystem.h:398` |

**[inferred], to verify in step S1:** spawn each cover block deferred, add a `USmartObjectComponent`, call `SetDefinition` with one runtime-built definition, then finish spawning. The fallback, if a cooked build rejects the runtime definition, is to create the definition asset with an editor Python script run through the queue.

## B4. StateTree events

| Fact | Source |
|---|---|
| `UStateTreeComponent::SendStateTreeEvent(Tag, Payload, Origin)` (and an `FStateTreeEvent` overload) | `GST/Public/Components/StateTreeComponent.h:116-120` |
| It refuses (with a warning) if the tree isn't running or has no asset | `GST/Private/Components/StateTreeComponent.cpp:577-589` |
| The event goes into a per-instance queue of at most **64** events; extra events are **dropped with an error** | `ST/Public/StateTreeEvents.h:176-181`; `ST/Private/StateTreeEvents.cpp:37-58` (drop at `:45-49`) |
| Sending an event **schedules a tick** with reason `Event`, so a sleeping tree wakes | `ST/Private/StateTreeExecutionContext.cpp:918-938` (`ScheduleNextTick(UE::StateTree::ETickReason::Event)` at `:935`) |
| Scheduled ticking (a tree may sleep, tick at a custom rate, or tick every frame) exists in 5.8: `FStateTreeScheduledTick::MakeSleep`, `MakeEveryFrames`, `MakeNextFrame`, `MakeCustomTickRate` | `ST/Public/StateTreeExecutionTypes.h:815-840` |
| For `UStateTreeComponent`, scheduled ticking is **allowed by default**: cvar `StateTree.Component.ScheduledTickEnabled` defaults to true, and the component schema's default policy reads `StateTree.Component.DefaultScheduledTickAllowed` (true) | `GST/Private/Components/StateTreeComponent.cpp:17-25`; `GST/Private/Components/StateTreeComponentSchema.cpp:24-28, 58-70` |
| When the next scheduled tick says "sleep", the component **disables its tick** | `GST/Private/Components/StateTreeComponent.cpp:298-330` (`SetComponentTickEnabled(false)` at `:316`) |
| The tree's permission is fixed at compile time from the schema | `ST/Private/StateTree.cpp:1024`; `ST/Public/StateTree.h:635` |
| A StateTree's runtime data is compiled from editor-only `EditorData` | `ST/Public/StateTree.h:381-384` |

**Consequences.**
1. In 5.8 a StateTree can genuinely be **event-driven**: it can sleep and be woken by `SendStateTreeEvent`. Whether a given tree sleeps depends on its tasks and transitions **[inferred]** (a task that ticks keeps it awake). The portfolio research quoted a 5.7 forum thread saying "StateTree ticks every frame by default" ([research/02](../Research/02_technical_challenges.md) [T9-C]). In 5.8 that is no longer a fixed rule: the component allows sleeping by default.
2. A StateTree asset can't be authored from runtime code (its compiled data comes from `EditorData`) **[inferred]**. Using StateTree in the benchmark would need an editor-authored asset. That is a decision for the owner (`01b` §8). v1 uses a small C++ brain and exposes an event the StateTree adapter can forward.
3. Events can be dropped when more than 64 are queued, so the squad system must send **one** event per assignment change, never one per frame.

## B5. AI Perception: sight is already time-sliced

`AIModule/Classes/Perception/AISense_Sight.h` and `AIModule/Private/Perception/AISense_Sight.cpp`:

| Fact | Source |
|---|---|
| The sight sense reads its limits from config (`config=Game`) | `AISense_Sight.h:203`, properties at `:244-277` |
| Defaults: `MaxTracesPerTick = 6`, `MaxAsyncTracesPerTick = 10`, `MinQueriesPerTimeSliceCheck = 40`, `MaxTimeSlicePerTick = 0.005` (5 ms), `bUseAsynchronousTraceForDefaultSightQueries = false` | `AISense_Sight.cpp:46-51, 134-145` |
| No override in `BaseGame.ini` or `BaseEngine.ini` | grep of `Config/BaseGame.ini` and `Config/BaseEngine.ini` for `AISense_Sight` finds nothing |
| Each update stops when the time slice is spent (checked every `MinQueriesPerTimeSliceCheck` queries) **or** the trace cap is hit | `AISense_Sight.cpp:295-297, 327-333` |
| Perception has its own CSV column `Exclusive/GameThread/AIPerception` | present in UE5-UpdateBudget's packaged captures (batch `2026-11-21_001816_s3smoke2`) |
| A team sense exists that broadcasts stimuli to teammates (`UAISense_Team`, `FAITeamStimulusEvent`) | `AIModule/Classes/Perception/AISense_Team.h:14, 50` |

**Consequence.** With 6 synchronous sight traces per tick by default, detection latency grows with the number of listener–target pairs **[inferred]** (an old forum report of 5–20 s delays with about 100 AIs is anecdotal: [research/02](../Research/02_technical_challenges.md) [T9-F]). If agents detected the threat through perception, time-to-cover would mix **perception latency** (identical in every configuration, but dependent on N) with **decision latency** (what we change). v1 therefore delivers the threat as a scripted squad event, and perception is out of the measured loop (`01` §8). `UAISense_Team` shows the engine already shares *sensing* within teams; it does not share *evaluation*.

## B6. Async traces (considered, not used in v1)

`Source/Runtime/Engine/Classes/Engine/World.h:2522` declares `AsyncLineTraceByChannel(EAsyncTraceType, Start, End, Channel, Params, ResponseParam, const FTraceDelegate*, UserData)`, and `:2708` `QueryTraceData(Handle, OutData)` ("return true if already done … false if either expired or not yet evaluated"). The engine's EQS trace test uses **synchronous** traces (`Tests/EnvQueryTest_Trace.cpp:93-104`, `World->LineTraceTestByChannel` in `:219-226`). Async traces would move trace cost off the game thread at the price of one frame of latency. Every configuration uses the engine's synchronous trace test in v1, so the comparison is like-for-like; async traces are a growth path (`02` §11).

## B7. Debugging tools available

| Tool | Fact | Source |
|---|---|---|
| Visual Logger | `ENABLE_VISUAL_LOG` is on for desktop builds with logging and debug drawing; EQS already logs to it (`UE_VLOG_EQS`) | `Source/Runtime/Engine/Public/EngineDefines.h:18-20`; `EnvQueryManager.cpp:506` |
| Gameplay Debugger | Compiled when the target enables it (`WITH_GAMEPLAY_DEBUGGER`); a Smart Object category ships | `Source/Runtime/GameplayDebugger/GameplayDebugger.Build.cs:47-62`; `SO/Public/GameplayDebuggerCategory_SmartObject.h` |
| Stats | EQS cycle stats `STAT_AI_EQS_Tick`, `…_TickWork`, `…_GeneratorTime`, `…_TestTime`, `…_NumInstances`, `…_AvgInstanceResponseTime` | `EnvQueryManager.cpp:40-54` |
