> **Raw Phase 2 research notes (A_eqs_internals), UE 5.8.4, 2026-11-21.** Working notes kept as an evidence trail. Every claim cites a `File:line` or a URL. **The claims the design depends on were re-checked by hand and are restated in [../01b_Research_Findings.md](../01b_Research_Findings.md) (marked ✔). Treat anything here that is not restated there as unverified.**

# P07 SquadQuery: Phase 2 research, part A: how EQS runs a query (UE 5.8.4)

- Engine: `C:\Program Files\Epic Games\UE_5.8\Engine`. `Build/Build.version` reads `"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 4, "Changelist": 58902052`.
- Paths below are relative to `Engine/`. `AIModule` means `Source/Runtime/AIModule`.
- **[inferred]** marks a conclusion drawn from following the code, not a direct statement in the source.

---

## A1. Who runs queries, and when

| Fact | Source |
|---|---|
| The EQS manager is an AI subsystem, and every AI subsystem is an `FTickableGameObject`. So EQS work happens in the tickables pass of the frame, not inside any actor's tick | `AIModule/Classes/EnvironmentQuery/EnvQueryManager.h:207` `class UEnvQueryManager : public UAISubsystem, public FSelfRegisteringExec`; `AIModule/Classes/AISubsystem.h:15` `class UAISubsystem : public UObject, public FTickableGameObject` |
| `RunQuery` does no work. It only appends the instance to `RunningQueries` and returns an id | `AIModule/Private/EnvironmentQuery/EnvQueryManager.cpp:287-303` (`RunningQueries.Add(QueryInstance)`) |
| `RunInstantQuery` runs a query to completion in the calling frame, with no time limit. The header warns it "Bypasses all EQS perf controlling and time slicing mechanics" | `EnvQueryManager.h:235-239`; `EnvQueryManager.cpp:318-347` (`ExecuteOneStep(UEnvQueryTypes::UnlimitedStepTime)` in a loop) |
| `PrepareQueryInstance` is public: a caller can create the instance itself, keep the `TSharedPtr<FEnvQueryInstance>`, and hand it to `RunQuery(const TSharedPtr<FEnvQueryInstance>&, …)` | `EnvQueryManager.h:224, 242`; `EnvQueryManager.cpp:377-404` |
| The finished callback receives `TSharedPtr<FEnvQueryResult>`. `FEnvQueryInstance` derives from `FEnvQueryResult`, so execution data (time, debug data) lives on the instance, not on the result type | `AIModule/Classes/EnvironmentQuery/EnvQueryTypes.h:593` `DECLARE_DELEGATE_OneParam(FQueryFinishedSignature, TSharedPtr<FEnvQueryResult>)`; `:789` `struct FEnvQueryInstance : public FEnvQueryResult`; `:859` `double TotalExecutionTime;` |

**Per querier.** Every query instance has one owner (`FEnvQueryResult::Owner`, `EnvQueryTypes.h:516` onward). `ExecuteOneStep` marks the query "owner lost" if the owner is gone (`AIModule/Private/EnvironmentQuery/EnvQueryInstance.cpp:294-298`). Nothing in the manager groups, merges or deduplicates instances of the same template for different owners: `RunningQueries` is a flat array of independent instances (`EnvQueryManager.h:311`). **[inferred]** from reading `Tick` and `RunQuery`: there is no code path that reuses one instance's items or scores for another instance.

## A2. Time-slicing: one budget for all queries, breadth first

`UEnvQueryManager::Tick` (`EnvQueryManager.cpp:434-642`):

```cpp
CSV_SCOPED_TIMING_STAT_EXCLUSIVE(EnvQueryManager);          // :440
double TimeLeft = MaxAllowedTestingTime;                     // :446
while ((TimeLeft > 0.0) && (Index < NumRunningQueries) && …) // :456-460
{
    QueryInstancePtr->ExecuteOneStep(TimeLeft);              // :482
    if (finished) { … FinishDelegate.ExecuteIfBound(QueryInstance); … }   // :518
    else if (bTestQueriesUsingBreadth) { ++Index; }          // :550-553
    if (bTestQueriesUsingBreadth && (Index == NumRunningQueries)) { Index = 0; }   // :567-570
    TimeLeft -= StepProcessingTime;                          // :577 (only when time slicing is allowed, :573)
}
```

| Fact | Source |
|---|---|
| **The budget is global, per frame, shared by every running query in the world**, not per querier | `EnvQueryManager.cpp:446` |
| The constructor sets `MaxAllowedTestingTime = 0.01f` and `bTestQueriesUsingBreadth = true` | `EnvQueryManager.cpp:182-183` |
| `BaseGame.ini` sets the same values, so **10 ms per frame, breadth first** is the shipped default | `Config/BaseGame.ini:230-232` |
| The config struct `FEnvQueryManagerConfig` has a different default, `0.003f`, and `bTestQueriesUsingBreadth = false`. It is only applied through `Configure(...)` | `EnvQueryManager.h:36, 42`; `EnvQueryManager.cpp:1156-1157` |
| Breadth first: one step of one query, then the next query; wrap to the first query at the end of the list | `EnvQueryManager.cpp:550-553, 567-570` |
| The finish delegate's time is subtracted from the step time, so **the consumer's handling does not eat into the EQS budget** | `EnvQueryManager.cpp:500-501` (comment), `:575-577` (`(Now - StepStartTime) - ResultHandlingDuration`) |
| Time slicing can be switched off in non-Shipping, non-Test builds (`SetAllowTimeSlicing`); it is on by default | `EnvQueryManager.h:284, 373-375`; `EnvQueryManager.cpp:57` |
| A CSV exclusive timing stat wraps the whole tick, and the instant-query path. It appears as the column `Exclusive/GameThread/EnvQueryManager` | `EnvQueryManager.cpp:331, 440`; the column is present in UE5-UpdateBudget's packaged Development captures on this machine (batch `2026-11-21_001816_s3smoke2`) |

**One step = the generator, or one test over as many items as fit.** `FEnvQueryInstance::ExecuteOneStep` (`EnvQueryInstance.cpp:292-521`):
- `CurrentTest < 0` runs the generator once (`:319-423`, `GenerateItems` at `:401`).
- Otherwise it runs one test (`:424-472`, `RunTest` at `:460`). The item iterator gets a deadline from the remaining budget: `Deadline = QueryInstance.CurrentStepTimeLimit > 0.0 ? (FPlatformTime::Seconds() + QueryInstance.CurrentStepTimeLimit) : -1.0;` (`:590`), and stops when it passes (`EnvQueryTypes.h:1221`). A test can therefore stop half-way through the items and **resume on the next frame** at `CurrentTestStartingItem` (`EnvQueryInstance.cpp:456, 464-466`).
- The budget is wall-clock time (`FPlatformTime::Seconds()`). **[inferred]** So even in a fixed-step `-deterministic` run, the frame in which a query finishes can differ between runs. Decisions are not bit-identical across runs, and the benchmark must treat run-to-run variation as real.

## A3. What is cached: templates, not results

| Fact | Source |
|---|---|
| `FEnvQueryInstanceCache` is documented as "cache of instances with sorted tests". It holds a duplicated `UEnvQuery` template, a prototype `FEnvQueryInstance`, and the asset name | `EnvQueryManager.h:128-144` |
| The cache key is the template's full name **and** the run mode | `EnvQueryManager.cpp:792-800` |
| On a miss, the template is duplicated into the manager (`StaticDuplicateObject`), its options and tests are duplicated and sorted, and a prototype instance is built | `EnvQueryManager.cpp:803-956`, `StaticDuplicateObject` at `:812` |
| Every request then gets a **new copy of the prototype**: `TSharedPtr<FEnvQueryInstance> NewInstance(new FEnvQueryInstance(*InstanceTemplate));` | `EnvQueryManager.cpp:966` |
| Within one instance, each context is evaluated once and cached for that instance only (`ContextCache`) | `EnvQueryTypes.h:811-813`; `EnvQueryInstance.cpp:117-137` |

**Conclusion:** the README's statement "EQS caches query templates, not results" is **correct** for 5.8.4. Item lists, test results and scores die with each instance. Two agents that run the same query in the same frame against the same threat compute everything twice. (This was also read on 5.8.3 in [research/04](../Research/04_local_engine_verification.md) §4d.)

## A4. Test ordering, and the single-result shortcut (this corrects the README)

| Fact | Source |
|---|---|
| Tests are sorted: filters before scoring tests, then cheaper before more expensive (`EEnvTestCost`) | `EnvQueryManager.cpp:741-753`, sort at `:945` |
| The trace test is `Cost = EEnvTestCost::High`; the distance test is `Low` | `AIModule/Private/EnvironmentQuery/Tests/EnvQueryTest_Trace.cpp:112`; `Tests/EnvQueryTest_Distance.cpp:55` |
| Generators sort tests automatically by default (`bAutoSortTests = true`) | `AIModule/Private/EnvironmentQuery/EnvQueryGenerator.cpp:9` |
| **In `SingleResult` mode, the most expensive filter is moved to run last** | `EnvQueryManager.cpp:887` (`bOptionSingleResultSearch`), `:906-912` (picks `MostExpensiveFilter`), `:946-953` (appends it last: "we want to perform a single most expensive test as the last test to accept the first one that passes the test") |
| Before that last test runs, the items are sorted by the scores of the earlier tests | `EnvQueryInstance.cpp:433-453` (`SortScores()` at `:451`) |
| The last test then **stops at the first item that passes** (`PickSingleItem`, `bFoundSingleResult = true`), and the iterator ends | `EnvQueryInstance.cpp:614-627`; `EnvQueryTypes.h:1221, 1224-1232` |
| The shortcut applies only if that test can be a final condition: a pure filter, or filter-and-score with a constant score | `AIModule/Classes/EnvironmentQuery/EnvQueryTest.h:156-161` |
| Discarded items are skipped by every later test | `EnvQueryTypes.h:1075-1080` |

**Consequence.** For the typical per-agent cover query ("nearest point the threat can't see", run as `SingleResult`), EQS does **not** trace every item. It scores all items by distance (cheap), sorts them, then traces from the nearest outwards and stops at the first hidden one. The per-agent cost is about

```text
items × (cheap tests)  +  k × (trace)      where k = items tried before the first hidden one
```

not `items × tests`. The README's "cost is agents × items × tests" is therefore an **overstatement** for single-result queries. It holds for `AllMatching` queries, and for queries whose expensive test also scores. The README and `01` are corrected. This also means a shared squad query (which must return all valid items, so it traces all of them) is **not automatically cheaper** than eight short-circuited per-agent queries. That becomes hypothesis H2 in `01`, to be measured.

## A5. Development-build overhead: the EQS debugger is on

| Fact | Source |
|---|---|
| `USE_EQS_DEBUGGER` is 1 in every build except Shipping and Test | `EnvQueryTypes.h:33` |
| `FEnvQueryInstance::bDebuggingInfoEnabled = true` by default, and every new instance copies it into `bStoreDebugInfo` | `EnvQueryInstance.cpp:107`; `AIModule/Private/EnvironmentQuery/EnvQueryTypes.cpp:355` |
| With debug info on, every step stores its execution time and processed item count (`DebugData.Store`), and every finished query is stored by the manager's debugger (`StoreStats`, `StoreQuery`) | `EnvQueryInstance.cpp:482-485`; `EnvQueryManager.cpp:508-511, 1170-1238` |
| `NumProcessedItems` is incremented once per item per test, and reset per step | `EnvQueryInstance.cpp:610, 492-494`; `EnvQueryTypes.h:837` |
| `bDebuggingInfoEnabled` is a public static, so it can be switched off at runtime in a Development build | `EnvQueryTypes.h:1315-1318` |

**Consequences.**
1. Our headline builds are packaged Development (the launcher engine can't build Test; UE5-UpdateBudget, `09` F-3a). They therefore include this per-query and per-step debugger cost, which Shipping does not. The cost is **per query**, so it inflates configurations that run many queries. Phase 8 must measure it (one batch with `bDebuggingInfoEnabled = false`) and report both.
2. The same debug data gives an **exact count of item tests**: the sum of `NumProcessedItems` over a finished query's steps. That is how the "item tests per second" metric is measured (Development only; documented as such).

## A6. Can a query template be built in C++ at runtime?

Yes, with public API only (no assets, no editor):

| Fact | Source |
|---|---|
| `UEnvQuery::GetOptionsMutable()` is public and not editor-only | `AIModule/Classes/EnvironmentQuery/EnvQuery.h:66` |
| `UEnvQueryOption::Generator` and `Tests` are public `UPROPERTY`s | `AIModule/Classes/EnvironmentQuery/EnvQueryOption.h` (members after `GENERATED_UCLASS_BODY()`, no access specifier) |
| The settings of the trace, distance and pathfinding tests and the grid and circle generators are public members (`TraceData`, `Context`, `TestMode`, `DistanceTo`, …) | `Tests/EnvQueryTest_Trace.h:18-40`; `Tests/EnvQueryTest_Distance.h:25-35`; `Generators/EnvQueryGenerator_SimpleGrid.h:18-32` |
| `UEnvQuery::PostInitProperties` sets `QueryName = GetFName()`, so a `NewObject<UEnvQuery>` gets a valid name | `AIModule/Private/EnvironmentQuery/EnvQuery.cpp:156-160` |
| The manager rejects a template with no options, and drops options whose generator is missing or invalid | `EnvQueryManager.cpp:781-785, 833-860` |
| The Smart Object generator's settings (`QueryOriginContext`, `QueryBoxExtent`, `bOnlyClaimable`) are **protected**. A subclass can set them in its constructor | `Plugins/Runtime/SmartObjects/Source/SmartObjectsModule/Public/EnvQueryGenerator_SmartObjects.h:19-39` |

**[inferred]** Building the benchmark's queries in C++ keeps every query visible in code review and identical across configurations. A shipping game would author the same query as an asset; the system accepts any `UEnvQuery`.

## A7. How games usually run per-agent EQS (the baseline we must imitate)

| Path | What it does | Source |
|---|---|---|
| Behavior Tree service "Run EQS query on a regular basis" | Re-runs one query per agent on an interval. Every service defaults to `Interval = 0.5f`, `RandomDeviation = 0.1f` | `AIModule/Private/BehaviorTree/Services/BTService_RunEQS.cpp:20`; `AIModule/Private/BehaviorTree/BTService.cpp:20-21` |
| StateTree task "Run Env Query" | Runs one query per agent when its state is entered, with the agent as query owner; the default run mode is `SingleResult` | `Plugins/Runtime/GameplayStateTree/Source/GameplayStateTreeModule/Public/Tasks/StateTreeRunEnvQueryTask.h:24, 36, 49`; `Private/Tasks/StateTreeRunEnvQueryTask.cpp:36` |
| After a pick, the agent moves with `AAIController::MoveToLocation`, which **finds the path synchronously** on the game thread | `AIModule/Private/AIController.cpp:612, 899-906` (`NavSys->FindPathSync(Query)`) |

So the polled baseline uses 0.5 s ± 0.1 s, the engine's own default, and every changed decision costs one synchronous path query. That makes decision switches a CPU cost, not just a quality problem.

## A8. Mass EQS (5.8) is not result sharing

`Plugins/Runtime/MassGameplay/Source/MassEQS/Public/MassEQSSubsystem.h` (`UMassEQSSubsystem : UMassSubsystemBase`): "Subsystem for sending UMassEQSGenerator and UMassEQSTest requests to Mass Processors, and for returning the results back to EQS." It moves generator and test work for **Mass entities** into processors. Each request still belongs to one query instance (`PushRequest(const FEnvQueryInstance& QueryInstance, …)`). It does not share results between queriers **[inferred]**, and it targets Mass entities, not Actor agents.
