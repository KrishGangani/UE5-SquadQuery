# 04 · C++ Fundamentals: SquadQuery

**Rule:** only the C++ (and build-language) ideas this project's code actually relies on, each with a tiny example first and then where it appears in our code. This file grows with every step. Sections 1–4 came with Phase 4, 5–9 with S1.

---

## 1. Two languages in one project: C++ for the game, C# for the build

```text
SquadQuery.Build.cs   ← C#, compiled and run by UnrealBuildTool before any C++ is compiled
SquadQueryModule.cpp  ← C++, compiled by MSVC into the game
```
- UnrealBuildTool (UBT) is a C# program. It **runs** the `*.Target.cs` and `*.Build.cs` files to learn what to build, then calls the C++ compiler.
- So a `.Build.cs` file is not configuration text: it is code. `new string[] { "Core", "Engine" }` is a C# array literal, and `AddRange` is a C# list method.
- **In our code:** `SquadQuery.Target.cs`, `SquadQueryEditor.Target.cs`, `SquadQuery.Build.cs`, `SquadQueryCore.Build.cs` (`05` A2, A3, A6).

## 2. The preprocessor: `#include` and macros

```cpp
#include "Modules/ModuleManager.h"     // paste the contents of that header here, before compiling
#define SQUARE(X) ((X) * (X))           // a text substitution, done before the compiler sees the code
int Nine = SQUARE(3);                    // the compiler actually sees ((3) * (3))
```
- `#include` copies a header's text into the file. That is how one `.cpp` sees declarations made elsewhere.
- A macro is **text replacement** with no type checking. Unreal uses macros to generate boilerplate the language can't express (module entry points now; `UCLASS`, `UPROPERTY` and `GENERATED_BODY` from S1).
- **In our code:** `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, SquadQuery, "SquadQuery")` expands to the functions and static objects that let the engine find and start the module (`06` §1). Because it is text, a typo in the module name doesn't fail at compile time; it fails when the engine can't find the module at start-up (`07`).

## 3. Static initialisation: code that runs before `main`

```cpp
struct FRegistrar { FRegistrar() { RegisterSomething(); } };
static FRegistrar GRegistrar;   // constructed when the program (or DLL) loads, before any normal code runs
```
- A global object with a constructor runs that constructor when its executable or DLL is loaded.
- **In our code:** in a packaged (monolithic) build, `IMPLEMENT_MODULE` registers the module this way, so the module manager knows about it without anyone calling it by name (`Source/Runtime/Core/Public/Modules/ModuleManager.h:955` onward). In the editor (modular) build the macro exports a function from the DLL instead (`:946`).

## 4. Inheritance in C#: `: TargetRules`, `: ModuleRules`, `: base(Target)`

```csharp
public class SquadQueryTarget : TargetRules            // "is a" TargetRules
{
    public SquadQueryTarget(TargetInfo Target) : base(Target) { Type = TargetType.Game; }
}
```
- `: TargetRules` means our class inherits every setting UBT knows about, with engine defaults; we only change the ones we care about.
- `: base(Target)` calls the parent's constructor first, so the defaults are in place before our lines run.
- **In our code:** every `Target.cs` and `Build.cs`. The same idea in C++ (`class UMySubsystem : public UWorldSubsystem`) appears from S1.

## 5. `static constexpr` class constants

```cpp
struct FBox3 { static constexpr float LengthCm = 300.f; };
constexpr float HalfLength = 0.5f * FBox3::LengthCm;   // computed by the compiler: 150
```
- `static`: the value belongs to the class, not to each object (no per-object memory).
- `constexpr`: known at compile time, so other compile-time constants can be built from it, and it can be used where the language needs a constant.
- **In our code:** `ASQCoverBlock::LengthCm/ThicknessCm/HeightCm` and the arena's layout constants. The slot offsets are derived from them in `SQArena::LongSideY` and `ShortSideX`, so the slots follow the block if its size changes.

## 6. `float` vs `double` for time

```cpp
double Start = FPlatformTime::Seconds();   // seconds since the process started, high resolution
float  Rate  = 0.1f;                         // a short interval: float is plenty
```
- A `float` has about 7 significant digits. A clock that has been running for hours needs more than that to keep millisecond precision, so absolute time stamps are `double`; short intervals and distances are `float`.
- **In our code:** `BuildStartRealSeconds` is `double`; the poll rate and all centimetre constants are `float`.

## 7. Named namespaces for file-local helpers

```cpp
namespace SQArena { constexpr float PollIntervalSeconds = 0.1f; bool IsPathComplete(UWorld&, const FVector&, const FVector&); }
```
- A namespace groups names so they can't collide with engine names or with another file's helpers.
- *Alternative:* an anonymous namespace (`namespace { … }`) makes the names invisible to other files. A named one reads better in explanations (`SQArena::CornerCm`) and, in a game module, the collision risk is the same.
- **In our code:** `SQArena` in `SQArenaSubsystem.cpp`.

## 8. The order of evaluation of function arguments is unspecified

```cpp
FVector P(Stream.FRandRange(0, 1), Stream.FRandRange(0, 1), 0);   // which draw becomes X? The standard doesn't say
const float X = Stream.FRandRange(0, 1);                         // fixed: one draw per statement
const float Y = Stream.FRandRange(0, 1);
```
- C++ guarantees each argument is evaluated before the call, but not in which order. Two calls with side effects in one argument list can run in either order, and a different compiler (or optimiser setting) may choose differently.
- With a seeded random stream, that would silently change the arena between builds.
- **In our code:** every random draw in `BuildArena` and `PlaceThreatPositions` has its own statement.

## 9. Lambdas and captures

```cpp
const FVector Candidate(X, Y, 0.f);
const bool bBlocked = Blocks.ContainsByPredicate([&Candidate](const ASQCoverBlock* Block)
{
    return FVector::DistSquared2D(Block->GetActorLocation(), Candidate) < 260.f * 260.f;
});
```
- A lambda is an unnamed function written where it is used. `[&Candidate]` **captures** `Candidate` by reference, so the lambda can read it without a copy; `[Candidate]` would copy it.
- Capturing by reference is safe here because the lambda runs immediately, inside `ContainsByPredicate`, while `Candidate` is alive. Storing such a lambda for later would leave it with a dangling reference.
- **In our code:** `PlaceThreatPositions` checks a candidate threat position against every block.

## 10. `static_assert`: a check the compiler runs

```cpp
constexpr float CornerCm = GridHalfExtentCm + GridJitterCm + ShortSideX + 290.f;   // 6200
static_assert(CornerCm < 0.5f * FloorSizeCm, "path-check corners must lie on the floor");
```
- `static_assert(condition, message)` is evaluated **while compiling**. If the condition is false, the build stops with the message. It costs nothing at run time.
- It only works on compile-time values, which is one more reason the layout numbers are `constexpr` (§5).
- **In our code:** if someone enlarges the grid or shrinks the floor so that the path-check corners would fall off the floor, the build fails instead of the self-check failing for a misleading reason.

## 11. References to arrays, and looping over a table of pairs

```cpp
constexpr int32 PathEnds[4][2] = { { 0, 1 }, { 2, 3 }, { 0, 3 }, { 2, 1 } };
for (const int32 (&Ends)[2] : PathEnds)
{
    CompletePaths += IsPathComplete(NavSys, Corners[Ends[0]], Corners[Ends[1]]) ? 1 : 0;
}
```
- `PathEnds` is an array of four elements, each itself an array of two `int32`. A range-based `for` visits each element.
- `const int32 (&Ends)[2]` reads "`Ends` is a reference to a `const` array of two `int32`". The parentheses matter: `const int32& Ends[2]` would be an array of references, which C++ doesn't allow. A reference avoids copying each pair (cheap here, but the habit matters for larger elements); `const auto& Ends` means the same and is shorter.
- **In our code:** the self-check's four corner-to-corner path tests, written once instead of four nearly identical lines (*review*).

## 12. Floating-point modes: the same code can round differently in two binaries

```text
Editor target (UnrealEditor-SquadQuery.dll):  /fp:precise   seed 1 -> arena hash 0x0EBBDFD8
Game target   (SquadQuery.exe, packaged):     /fp:fast      seed 1 -> arena hash 0x1FFC2AF6   (same layout)
```
- `float` and `double` arithmetic rounds after every operation. Under `/fp:precise` the compiler must keep the operations in the order and precision the source states. Under `/fp:fast` it may reorder them, fuse a multiply and an add into one instruction (FMA, rounded once instead of twice), or keep intermediates at a different precision. Each choice is usually a last-bit difference.
- UE 5.8 picks the mode per target type: `/fp:precise` for the editor, `/fp:fast` for the game (`01b` #21). So the editor binary running `-game` and the packaged game can produce numbers that differ in the last bit from identical source and identical random numbers.
- A hash of the raw bytes sees that difference; a human-level property (how many slots are hidden, whether paths exist) doesn't.
- **In our code:** the arena hash proves repeatability **within** one binary (two runs, same hash). Every measured run uses the packaged build, so comparisons never cross the two modes.

## 13. Shared ownership: `TSharedPtr`

```cpp
TSharedPtr<FEnvQueryInstance> RunningQuery = Manager->PrepareQueryInstance(Request, EEnvQueryRunMode::SingleResult);
Manager->RunQuery(RunningQuery, QueryFinishedDelegate);   // the manager keeps its own TSharedPtr to the same instance
// … later, when the query has finished:
RunningQuery.Reset();                                      // our reference goes; the instance lives while the manager holds one
```
- A `TSharedPtr<T>` counts how many owners an object has and deletes it when the last one lets go. It is for plain C++ objects (not UObjects, which the garbage collector owns).
- Here two systems hold the same query instance: the EQS manager, which steps it, and the brain, which reads its debug data when it finishes. Neither has to know when the other is done.
- `Reset()` drops one owner; `IsValid()` asks whether this pointer still points at something. `TSharedRef` is the never-null variant.
- **In our code:** `USQAgentBrainComponent::RunningQuery` (S3) and, from S5, the squad's running query.

## 14. Constant lookup tables

```cpp
namespace SQBench
{
	struct FModeName { const TCHAR* Name; ESQBrainMode Mode; };
	constexpr FModeName Modes[] = {
		{ TEXT("Scripted"), ESQBrainMode::Scripted },
		{ TEXT("B0"), ESQBrainMode::PolledEQS },
		// …
	};
}
for (const SQBench::FModeName& Entry : SQBench::Modes) { if (Mode == Entry.Name) { … } }
```
- A small array of plain structs, built entirely at compile time (`constexpr`): string literals and enum values are compile-time constants. Nothing is allocated or initialised at run time.
- One table serves both directions: parsing a name, and listing the valid names in an error message, so the two can't disagree.
- A `TMap` would be overkill for four entries; a linear scan of a tiny array is faster than hashing and needs no construction.
- **In our code:** the `-SQMode=` names in `SQBenchmarkSubsystem.cpp` (`05` D7).

## 15. The assignment problem and the Hungarian method, in plain steps

```text
cost   slot A  slot B          greedy, agent order 1, 2:   1 -> A (1),  2 -> B (9)   total 10
agent 1   1      2             optimal:                    1 -> B (2),  2 -> A (2)   total 4
agent 2   2      9
```
- **The problem.** n agents, m slots, a cost for every pair. Give each agent at most one slot and each slot at most one agent, so the total cost is smallest. Choosing agent by agent ("greedy") depends on the order and can be far from optimal, as above.
- **The idea (shortest augmenting path).** Add agents one at a time. For the new agent, search for the cheapest way to make room: take a free slot, or take a slot from an agent who then moves to another, and so on, along a chain that ends at a free slot. That chain is an *augmenting path*; flipping it places one more agent.
- **Why the search is a shortest-path search.** Each row and column carries a number, its *potential*. Measured in *reduced cost* (cost − row potential − column potential), every edge is ≥ 0, so Dijkstra's algorithm finds the cheapest chain. After each step the potentials are adjusted so that this stays true. When every agent is placed, no cheaper assignment exists.
- **Cost.** O(n² · (m + n)) for n agents and m slots: for 8 agents and 128 slots (plus 8 "no slot" columns), at most 8 × 8 × 136 ≈ 8,700 steps of the inner loop, a few microseconds.
- **The two tricks used here.** "No slot" columns at a high fixed price, so an agent can always be left unassigned instead of the search failing. And forbidden pairs as missing edges, never relaxed, so they can never appear in the answer.
- **In our code:** `FSquadAssignmentSolver::Solve` (`05` E2), and its Python prototype, checked against brute force before the C++ existed.

## 16. Raw pointers over a container, on purpose

```cpp
TArray<double, TInlineAllocator<160>> MinReduced;  MinReduced.SetNumUninitialized(Columns + 1);
double* const MinR = MinReduced.GetData();         // the container still owns the memory
for (int32 Column = 0; Column <= Columns; ++Column) { MinR[Column] = TNumericLimits<double>::Max(); }
```
- `Array[i]` on a `TArray` checks that `i` is in range, in every Development build. That is what you want almost everywhere.
- In a tight loop that runs hundreds of thousands of times per call, and whose indices are in range by construction, the checks are measurable. Taking the raw pointer once (`GetData()`) removes them for that loop only.
- The container still owns the memory. The pointer is valid until the array is resized, which this function never does after taking it.
- **In our code:** only in the solver's inner loops (`05` E2).
