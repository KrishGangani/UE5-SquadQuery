# 06 · Unreal APIs And Macros: SquadQuery

**Rule:**
- Only APIs this project **actually uses** appear here, each in the same step it enters the code.
- Each entry follows the 8-point format: what it is · why it exists · how it works · when to use it · when not to · common mistakes · a small example · how we use it.
- Verified against UE **5.8.4** source; file references are relative to `Engine/`.

| # | API | First used in |
|---|---|---|
| 1 | `IMPLEMENT_PRIMARY_GAME_MODULE` / `IMPLEMENT_MODULE`, `FDefaultGameModuleImpl` / `FDefaultModuleImpl` | Phase 4 |
| 2 | `ModuleRules`: dependency lists and `PCHUsage` | Phase 4 |
| 3 | `TargetRules`: `Type`, `DefaultBuildSettings`, `IncludeOrderVersion` | Phase 4 |
| 4 | Descriptors: `LoadingPhase`, and plugin dependencies in `.uproject` / `.uplugin` | Phase 4 |
| 5 | Config files: `config=Engine` / `config=Game` classes and their `Default*.ini` sections | Phase 4 |
| 6 | `UCLASS`, `GENERATED_BODY`, the `.generated.h` include | S1 |
| 7 | `UPROPERTY` object references, `TObjectPtr` | S1 |
| 8 | `CreateDefaultSubobject`, `ConstructorHelpers::FObjectFinder`, `LoadObject`, `NewObject`, `SpawnActorDeferred` + `FinishSpawning` | S1 |
| 9 | Timers: `FTimerManager::SetTimer` / `ClearTimer` | S1 |
| 10 | Delegates: native `FSimpleMulticastDelegate`, and binding an engine dynamic delegate | S1 |
| 11 | `UWorldSubsystem` | S1 |
| 12 | Smart Objects at runtime: definition, component, subsystem | S1 |
| 13 | Navigation at runtime: bounds volume, cached bounds, the build-finished event, path tests, projection | S1 |
| 14 | Scene queries: `LineTraceTestByChannel`, collision profiles | S1 |
| 15 | Core utilities: `FRandomStream`, `FCrc`, `TBitArray`, `FParse`, `FPlatformTime`, `RequestExit`, log macros | S1 |
| 16 | Characters and AI movement: `ACharacter`, `MoveToLocation`, `AutoPossessAI` | S2 |
| 17 | `UTickableWorldSubsystem` | S2 |
| 18 | The CSV profiler: categories, events, custom and timing stats, metadata | S2 |
| 19 | Files and paths: `FFileHelper::SaveStringToFile`, `FPaths::ProjectSavedDir` | S2 |
| 20 | View targets and class defaults: `ACameraActor`, `SetViewTarget`, `bAutoManageActiveCameraTarget`, `GetDefault<T>()` | S2 |
| 21 | Game modes: `AGameModeBase`, `DefaultPawnClass`, `GlobalDefaultGameMode` | S2 |
| 22 | EQS from C++: queries, options, generators, tests, contexts | S3 |
| 23 | Running queries: `PrepareQueryInstance`, `RunQuery`, `AbortQuery`, EQS debug data | S3 |
| 24 | Smart Object claims: `CanBeClaimed`, `MarkSlotAsClaimed`, `MarkSlotAsFree` | S3 |
| 25 | Tick intervals and the first tick | S3 |
| 26 | `TWeakObjectPtr`, `TOptional` | S3 |
| 27 | Automation tests: `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, flags, `TestEqual`, `AddInfo` | S4 |
| 28 | `TConstArrayView`, `ensureMsgf` | S4 |
| 29 | `TInlineAllocator` | S4 |

Entries 6–15 entered with S1 (the arena). EQS, AI controllers and the CSV profiler enter with S2 and S3.

---

## 1. `IMPLEMENT_PRIMARY_GAME_MODULE` / `IMPLEMENT_MODULE`

1. **What it is.** Macros that generate a module's entry point (`Source/Runtime/Core/Public/Modules/ModuleManager.h:946-955` for `IMPLEMENT_MODULE`, `:1100-1136` for the primary game variant).
2. **Why it exists.** The engine loads code as **modules**: DLLs in editor builds, static code in packaged (monolithic) builds. Each module needs a standard way to be found and started, so the module manager can create it and call `StartupModule()` / `ShutdownModule()`.
3. **How it works.**
   - In a modular build it exports an `InitializeModule` function that the module manager calls after loading the DLL; it returns a new instance of the class you name.
   - In a monolithic build it registers a static initialiser instead.
   - The *primary* variant also registers the signing key, encryption key and target name (`ModuleManager.h:1130-1135`) and, in monolithic builds, project-wide globals.
4. **When to use it.** Exactly one `IMPLEMENT_MODULE` (or a game variant) per module, and exactly one `IMPLEMENT_PRIMARY_GAME_MODULE` per project.
5. **When not to.** Not twice in one module (duplicate symbols at link time). Not the primary variant in a plugin.
6. **Common mistakes.**
   - The name argument doesn't match `Build.cs` and the descriptor: "module not found" at start-up.
   - Writing an empty custom `IModuleInterface` class when `FDefaultModuleImpl` would do.
   - Believing the third argument of the primary variant still names the game. In 5.8 it is `DEPRECATED_GameName` in monolithic builds (`ModuleManager.h:1100, 1114`) and unused in modular ones (`:1130-1131`).
7. **Example.**
   ```cpp
   #include "Modules/ModuleManager.h"
   IMPLEMENT_MODULE(FDefaultModuleImpl, MyPluginModule);
   ```
8. **In this project.** `SquadQueryModule.cpp` uses the primary variant with `FDefaultGameModuleImpl` (`ModuleManager.h:892`); `SquadQueryCoreModule.cpp` uses `IMPLEMENT_MODULE(FDefaultModuleImpl, SquadQueryCore)` (`:884`). Neither module needs start-up logic: everything is a subsystem or an actor that the engine creates itself.

## 2. `ModuleRules`: dependencies and `PCHUsage`

1. **What it is.** The C# class in `<Module>.Build.cs` that tells UnrealBuildTool how to compile one module.
2. **Why it exists.** Unreal is hundreds of modules. Each declares what it depends on, so UBT can work out include paths, link order and what to rebuild.
3. **How it works.**
   - `PublicDependencyModuleNames`: their include paths are given to us **and** to every module that depends on us.
   - `PrivateDependencyModuleNames`: only to us.
   - Both link the dependency.
   - `PCHUsage = UseExplicitOrSharedPCHs`: use a shared precompiled header built for a common dependency set unless the module names its own. It is the default since `BuildSettingsVersion.V2` (`Source/Programs/UnrealBuildTool/Configuration/Rules/TargetRules.cs:131-135`).
4. **When to use which.** Public **only** if one of *your public headers* includes that module's headers. Everything else is private.
5. **When not to.** Don't add modules "just in case". Every dependency costs build time and couples modules.
6. **Common mistakes.**
   - Making everything public: wider include paths, wider rebuilds.
   - A missing dependency: `unresolved external symbol` at link time or `cannot open include file`.
   - A dependency on a **plugin** module without declaring the plugin: UBT warns (`UEBuildTarget.cs:1560-1580` for plugins, `:1660-1690` for projects). See §4.
   - A circular dependency between two modules (UBT reports it).
7. **Example.**
   ```csharp
   PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
   PrivateDependencyModuleNames.Add("SquadQueryCore");
   ```
8. **In this project.** The game module depends **privately** on `SquadQueryCore` until a public game header needs a core type (S3, recorded in `05`). The plugin depends publicly on `Core`, `CoreUObject` and `Engine`, because its public headers will expose engine types. `AIModule`, `SmartObjectsModule`, `NavigationSystem`, `GameplayTags` and `DeveloperSettings` are added in the steps that first include them.

## 3. `TargetRules`: `Type`, `DefaultBuildSettings`, `IncludeOrderVersion`

1. **What it is.** The C# class in `<Name>.Target.cs` that describes one executable: game, editor, client, server or program.
2. **Why it exists.** The same modules build into different executables, each with its own settings and module set.
3. **How it works.** UBT finds the class `<Name>Target` and builds its `ExtraModuleNames` plus the engine and enabled plugins. `DefaultBuildSettings` selects a versioned set of default build options; `IncludeOrderVersion` selects which engine version's header include order to use.
4. **When to use it.** One file per executable type. Here: `SquadQuery` (game) and `SquadQueryEditor` (editor).
5. **When not to.** Don't add a `Server` or `Client` target the project doesn't need. This project is single-player.
6. **Common mistakes.**
   - A class name that doesn't end in `Target`, or doesn't match the file name: UBT can't find the target.
   - An old `DefaultBuildSettings` copied from a 4.x project: older defaults, and deprecation warnings.
   - Forgetting `ExtraModuleNames`: the game module isn't built into the target.
7. **Example.** See `05` A2.
8. **In this project.** Both targets use **V7** and **Unreal5_8**, the values in Epic's 5.8 `TP_Blank` template (`Templates/TP_Blank/Source/TP_Blank.Target.cs`), and the portfolio standard.

## 4. Descriptors: `LoadingPhase` and plugin dependencies

1. **What it is.** The JSON fields in `.uproject` and `.uplugin` that list modules (with a type and a loading phase) and the plugins a project or plugin needs.
2. **Why it exists.** The engine has to know which modules to load, in which order, and which plugins must be enabled, **before** any C++ runs.
3. **How it works.**
   - `LoadingPhase` orders module loading: `PreDefault`, then `Default`, then `PostDefault` (`Source/Runtime/Projects/Public/ModuleDescriptor.h:44-50`), among others for earlier engine phases.
   - A `"Plugins"` entry in a `.uplugin` makes the listed plugins load with it and tells UBT the dependency is intended. UBT warns if a plugin's module depends on another plugin's module without that entry (`UEBuildTarget.cs:1560-1580`).
   - A `"Plugins"` entry in the `.uproject` enables a plugin for the project. UBT warns if a project module depends on a plugin module that the project doesn't list (`UEBuildTarget.cs:1660-1690`).
4. **When to use it.** Always declare the plugins your modules link against, in the descriptor of the thing that links.
5. **When not to.** Don't enable plugins nothing uses; each one adds modules to load and content to cook.
6. **Common mistakes.**
   - Relying on `EnabledByDefault` alone for a plugin the game module links against (it works, but UBT warns).
   - Forgetting that some engine plugins are **off by default**. SmartObjects is one (`Plugins/Runtime/SmartObjects/SmartObjects.uplugin:13`).
   - Giving a module that registers classes a loading phase that comes after the first world is created.
7. **Example.**
   ```json
   "Plugins": [ { "Name": "SmartObjects", "Enabled": true } ]
   ```
8. **In this project.** The `.uproject` enables `SquadQueryCore` and `SmartObjects`, and **disables** `AndroidFileServer`, an engine plugin that is on by default and whose editor module writes a generated security token into the project's config (`05` A1). The `.uplugin` declares SmartObjects as a dependency of SquadQueryCore. Both modules load at `Default`; SmartObjects' runtime module loads at `PreDefault`, so it is ready first.

## 5. Config files: `config=Engine` / `config=Game` and `Default*.ini`

1. **What it is.** A `UCLASS(config=X)` reads its `UPROPERTY(config)` values from the config hierarchy for file `X` (Engine, Game, Input, …): engine `Base*.ini`, then the project's `Default*.ini`, then saved user settings. Section name: `[/Script/<Module>.<ClassWithoutPrefix>]`.
2. **Why it exists.** Values can change without recompiling, per project and per platform.
3. **How it works.** When the class default object is created, the engine loads the values in its section. A subclass's section overrides the values it inherits from the parent's section. That rule was not traced in the config code; the one case this project depends on was **checked at run time** instead: with `RuntimeGeneration=Static` for `NavigationData` in `BaseEngine.ini` and `Dynamic` for `RecastNavMesh` in our `DefaultEngine.ini`, `getall RecastNavMesh RuntimeGeneration showdefaults` prints `Dynamic`, uncooked and packaged (`09` R-4, R-7).
4. **When to use it.** For settings that a project, not a single instance, should decide.
5. **When not to.** Not for values that must differ per actor; those are properties on the actor or component.
6. **Common mistakes.**
   - **Putting a section in the wrong file.** The EQS manager is `config = Game` (`AIModule/Classes/EnvironmentQuery/EnvQueryManager.h:206`), so its section only works in `DefaultGame.ini`; written in `DefaultEngine.ini` it is silently ignored **[inferred from the config rules]**.
   - Using the class name with its `U`/`A` prefix in the section name.
   - Expecting a config change to apply to objects that already exist.
7. **Example.**
   ```ini
   [/Script/NavigationSystem.RecastNavMesh]
   RuntimeGeneration=Dynamic
   ```
8. **In this project.** `DefaultEngine.ini` holds the navigation keys (`RecastNavMesh` and `NavigationSystemV1` are `config=Engine`: `Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:570`, `NavigationSystem.h:290`). `DefaultGame.ini` holds the EQS slice. `DefaultInput.ini` holds the Enhanced Input classes (`05` A7–A9).

## 6. `UCLASS`, `GENERATED_BODY` and the `.generated.h` include

1. **What it is.** Macros read by UnrealHeaderTool (UHT), which writes extra C++ for every reflected class into `<File>.generated.h` and a generated `.cpp`.
2. **Why it exists.** C++ has no runtime reflection. Unreal's garbage collector, serialiser, editor and Blueprint need to know a class's properties and functions at run time, so UHT generates that description.
3. **How it works.** `UCLASS(specifiers)` marks the class; `GENERATED_BODY()` inside it expands to the generated declarations (`StaticClass()`, the object-system constructors). The generated header must be the **last** `#include` of the file, because it relies on everything declared before it.
4. **When to use it.** Any class deriving from `UObject` (actors, components, subsystems, data assets).
5. **When not to.** Plain data that no system needs to reflect: `FSQCoverSlot` is a plain struct for that reason (`05` B4).
6. **Common mistakes.** The generated include not last (UHT error); a file name that doesn't match the `.generated.h` name; adding `_API` exports to classes no other module uses.
7. **Example.** `UCLASS() class AMyActor : public AActor { GENERATED_BODY() public: AMyActor(); };`
8. **In this project.** `USQCoverBehaviorDefinition` (`HideDropdown, NotBlueprintType`: an internal type), `ASQCoverBlock` and `USQArenaSubsystem` (plain `UCLASS()`, no export macro, as in UE5-UpdateBudget's game module).

## 7. `UPROPERTY` object references and `TObjectPtr`

1. **What it is.** `UPROPERTY()` on a member makes it part of reflection; for a UObject pointer that means the garbage collector **sees** the reference. `TObjectPtr<T>` is the UE5 member-pointer type for UObjects.
2. **Why it exists.** Unreal's GC frees any UObject that nothing reachable references. A raw `UObject*` member that isn't a `UPROPERTY` is invisible to GC, so the object can be freed while you still point at it.
3. **How it works.** GC walks every reflected reference from the root set. `TObjectPtr` behaves like a raw pointer in game code, and adds access tracking in editor builds.
4. **When to use it.** Every UObject pointer a class keeps beyond one function.
5. **When not to.** Non-UObject data; temporary local pointers.
6. **Common mistakes.** Holding a runtime-created object (like a definition built with `NewObject`) only through a raw pointer; forgetting that a referenced object stays alive as long as the reference does.
7. **Example.** `UPROPERTY(Transient) TObjectPtr<UMyData> Data;`
8. **In this project.** `CoverDefinition` and `Blocks` in the arena subsystem are `UPROPERTY(Transient)`: the definition exists only at runtime, and this is the one strong reference GC can see. Specifiers used: `Transient` (never saved), `VisibleAnywhere` with `Category` on the block's components (shown in the editor, not replaceable).

## 8. Creating objects and actors: `CreateDefaultSubobject`, `ConstructorHelpers::FObjectFinder`, `LoadObject`, `NewObject`, `SpawnActorDeferred` + `FinishSpawning`

1. **What it is.** The ways this project creates things: components in a constructor, assets found in a constructor or loaded at run time, plain objects, and actors in two phases.
2. **Why it exists.** Each runs at a different time in an object's life. Components belong to the class template; actors need a world; a deferred spawn exists so you can configure an actor after construction but before its construction script and `BeginPlay` run.
3. **How it works.**
   - `CreateDefaultSubobject<T>(Name)` (constructor only) creates a component that every instance gets.
   - `ConstructorHelpers::FObjectFinder<T>(Path)` (constructor only) finds an asset; made `static`, it runs once per class.
   - `LoadObject<T>(Outer, Path)` loads an asset synchronously at any time.
   - `NewObject<T>(Outer, Name, Flags)` creates a plain UObject.
   - `UWorld::SpawnActorDeferred<T>(Class, Transform)` (`Engine/Classes/Engine/World.h:3851`) constructs the actor and, **if it has a native root component, already registers its components** (`Engine/Private/Actor.cpp:4330-4336`; only a Blueprint class without a native root defers registration). `AActor::FinishSpawning(Transform)` (`Engine/Classes/GameFramework/Actor.h:3117`) then runs the construction script and `PostActorConstruction`, which calls `BeginPlay` if the world has begun play (`Actor.cpp:4415-4420, 4509`).
   - `UWorld::SpawnActor<T>(Class, Transform)` does both steps at once.
4. **When to use it.** Deferred spawning whenever something must be configured before the construction script or `BeginPlay` runs (here: a Smart Object component registers with its subsystem in `BeginPlay`).
5. **When not to.** Synchronous loads during a measured run; `FObjectFinder` outside constructors (it asserts).
6. **Common mistakes.** Believing a deferred spawn delays registration (for native-root actors it doesn't, `07` S1); swapping the mesh of a registered **Static** component after the world has begun play (refused, `StaticMeshComponent.cpp:2374-2383`); forgetting `FinishSpawning` (no construction script, no `BeginPlay`); scaling an actor whose child offsets must stay in centimetres.
7. **Example.** `AMyActor* A = World.SpawnActorDeferred<AMyActor>(AMyActor::StaticClass(), T); A->Configure(); A->FinishSpawning(T);`
8. **In this project.** Blocks are spawned deferred so their Smart Object definition is set before `BeginPlay` registers the component with the Smart Object subsystem. The floor and the nav bounds volume use a plain `SpawnActor`: deferral would change nothing for them (*corrected in review*; the first version claimed it set their mesh and box before registration). The floor's mesh can still be set afterwards because the arena is built in `OnWorldBeginPlay`, before the world has begun play.

## 9. Timers: `FTimerManager::SetTimer` / `ClearTimer`

1. **What it is.** The world's timer facility: call a function after a delay or repeatedly.
2. **Why it exists.** Work that runs occasionally shouldn't need a per-frame tick.
3. **How it works.** `SetTimer(Handle, Object, &Class::Method, Rate, bLoop)` wraps the method in a UObject delegate (`Engine/Public/TimerManager.h:167-169`). The manager checks the delegate is still bound before calling it (`Engine/Private/TimerManager.cpp:152`), so a destroyed object is skipped. Time is world time.
4. **When to use it.** Delays, timeouts, periodic work with no per-frame need. Prefer an engine event over polling when one exists (§10, §13).
5. **When not to.** Work that must happen at a fixed point in the frame (use a tick function); work timed in real time.
6. **Common mistakes.** Losing the handle and never clearing a looping timer; assuming timers run in real time (in a fixed-step run with fast frames, world time runs ahead of the wall clock).
7. **Example.** `World->GetTimerManager().SetTimer(Handle, this, &UMyThing::OnTimeout, 60.f, /*bLoop*/ false);`
8. **In this project.** One timer: the arena's navigation timeout. It is a one-shot world-time timer that re-arms itself with the real time still left (`FPlatformTime`) until 60 real seconds have passed, and it is cleared when the arena is ready and in `Deinitialize`. *Changed in review:* the first version polled navigation every 0.1 s; readiness now comes from an engine event (§13).

## 10. Delegates: native `FSimpleMulticastDelegate`, and binding an engine dynamic delegate

1. **What it is.** A list of callbacks with no parameters (`Core/Public/Delegates/Delegate.h:377`). Its reflected cousin, the **dynamic** multicast delegate (`DECLARE_DYNAMIC_MULTICAST_DELEGATE_*`), stores its bindings as an object plus a function **name**, so it can be serialised and bound from Blueprint.
2. **Why it exists.** One object announces an event; any number of others listen, without the announcer knowing them.
3. **How it works.** Native: `AddUObject`/`AddLambda` bind; `Broadcast()` calls every binding; UObject bindings are skipped once their object is gone. Dynamic: `AddDynamic(this, &UMyClass::Handler)` / `AddUniqueDynamic` (never twice) bind a function that must be a `UFUNCTION()`, because the call goes through reflection by name; `RemoveAll(this)` unbinds.
4. **When to use it.** Native for C++-only events of your own; dynamic when Blueprint must bind, or when the engine only offers a dynamic one.
5. **When not to.** Exactly one listener (a single-cast delegate is clearer); a dynamic delegate on a hot path (a reflection call per binding).
6. **Common mistakes.** Binding after the event already fired (check `IsReady()` first, or bind before anything can fire it); a dynamic handler without `UFUNCTION()` (the bind fails to find it); broadcasting while listeners can destroy the announcer.
7. **Example.** `Arena->OnArenaReady.AddUObject(this, &UDirector::HandleArenaReady);` and `NavSys->OnNavigationGenerationFinishedDelegate.AddUniqueDynamic(this, &UMyThing::HandleNavBuilt);`
8. **In this project.** `USQArenaSubsystem::OnArenaReady` (native) fires once; the director (S2) binds to it, or calls straight through if the arena is already ready. The arena itself binds the navigation system's dynamic `OnNavigationGenerationFinishedDelegate` (§13) in `OnWorldBeginPlay`, before it spawns anything, and unbinds in `Deinitialize`.

## 11. World subsystems: `UWorldSubsystem`

1. **What it is.** A UObject the engine creates once per world and destroys with it (`Engine/Public/Subsystems/WorldSubsystem.h`).
2. **Why it exists.** World-wide services without a placed actor, and without a singleton that outlives the world.
3. **How it works.** `Initialize` → `PostInitialize` → `OnWorldBeginPlay(UWorld&)` (`:43`, before any actor's `BeginPlay`) → … → `Deinitialize`. `DoesSupportWorldType` (`:66`) chooses which world types get one.
4. **When to use it.** One service per world: here, building the arena.
5. **When not to.** State that must survive a level change (game-instance subsystem); per-actor behaviour (component).
6. **Common mistakes.** Spawning in `Initialize` (too early, the world isn't playing); forgetting that editor worlds get subsystems too.
7. **Example.** `UCLASS() class UMyWorldService : public UWorldSubsystem { … virtual void OnWorldBeginPlay(UWorld&) override; };`
8. **In this project.** `USQArenaSubsystem` builds in `OnWorldBeginPlay` and supports `Game` and `PIE` only.

## 12. Smart Objects at runtime: definition, component, subsystem

1. **What it is.** `USmartObjectDefinition` (a data asset: slots and their behaviors), `USmartObjectComponent` (puts a definition into the world), `USmartObjectSubsystem` (the runtime: registration, spatial queries, claims).
2. **Why it exists.** Shared, reservable interaction points with a spatial index, so agents can find and reserve them.
3. **How it works.**
   - A definition is valid only if every slot has a behavior definition (`SO/Private/SmartObjectDefinition.cpp:205-222`); `DebugAddSlot()` adds a slot from code (`SO/Public/SmartObjectDefinition.h:326-327`).
   - A component registers in `BeginPlay` in game worlds; before the runtime is initialised it is queued (`SO/Private/SmartObjectSubsystem.cpp:842-848`); the definition is validated at registration (`:749-770`).
   - `GetAllSlots(ObjectHandle, OutSlots)` lists a registered object's slots (`SmartObjectSubsystem.h:536`); `GetSlotTransformChecked(SlotHandle)` gives a slot's world transform (`:861`); `FindSmartObjects(Request, OutResults, UserData)` searches a box (`:473`).
4. **When to use it.** Anything agents reserve: seats, doors, cover.
5. **When not to.** Points nobody reserves (a plain array is cheaper).
6. **Common mistakes.** A slot without a behavior definition (the component silently doesn't register); changing a definition after registration; expecting the request box to test slot positions (it tests the object's location, `:2880`); expecting claimed slots in results (excluded by default, `01b` #18).
7. **Example.** `FSmartObjectSlotDefinition& S = Def->DebugAddSlot(); S.Offset = FVector3f(0, 110, 0); S.BehaviorDefinitions.Add(NewObject<UMyBehavior>(Def));`
8. **In this project.** One runtime definition with six slots, a component per block, and `GetAllSlots` to fill the arena's slot table. The self-check uses `FindSmartObjects` and `GetSlotTransformChecked` to prove the runtime agrees with the table.

## 13. Navigation at runtime: bounds, build state, paths, projection

1. **What it is.** `ANavMeshBoundsVolume` (where to build), `UNavigationSystemV1::OnNavigationBoundsUpdated` (announce bounds), `OnNavigationGenerationFinishedDelegate` (a build finished), `IsNavigationBuildInProgress` (is anything still queued?), `GetDefaultNavDataInstance` (the navmesh), `TestPathSync` (does a complete path exist?) and `ProjectPointToNavigation` (the nearest navmesh point).
2. **Why it exists.** Agents need a navmesh, built where you say and when geometry exists.
3. **How it works.** A registered bounds volume queues a bounds update with the volume's **cached** component bounds (`NS/Private/NavigationSystem.cpp:4145`); the navigation system's tick applies it, spawns the navmesh if none exists and marks tiles dirty; worker threads rebuild them (`01b` #9; `NS/Private/NavigationSystem.cpp:4301-4392`). When the generator's task count drops to zero it broadcasts `OnNavigationGenerationFinishedDelegate` (`01b` #20), for every build, however small. `TestPathSync(FPathFindingQuery(…))` (`NS/Public/NavigationSystem.h:679`) answers yes/no without creating a `UNavigationPath` object, and treats a partial path as no. `RuntimeGeneration=Dynamic` keeps the navmesh rebuildable at runtime (`05` A7).
4. **When to use it.** Runtime-built levels, procedural content.
5. **When not to.** Authored levels with a baked navmesh (Static is cheaper).
6. **Common mistakes.** Trusting "build not in progress" right after the bounds appear (nothing queued yet); trusting the build-finished event alone (it fires for a near-empty build too, `07` S1); a volume with no bounds outside the editor (no brush); giving a registered volume its box without `UpdateBounds()` (the cached bounds stay a point); path endpoints inside obstacles.
7. **Example.** `const ANavigationData* Nav = NavSys.GetDefaultNavDataInstance(); bool bOk = Nav && NavSys.TestPathSync(FPathFindingQuery(nullptr, *Nav, A, B));`
8. **In this project.** One runtime volume with a box body and an explicit `UpdateBounds()` (`05` B5); readiness = the build-finished event + nothing queued + a complete corner-to-corner path; the self-check tests four paths and projects all 600 slots onto the navmesh.

## 14. Scene queries: `LineTraceTestByChannel`, `FCollisionQueryParams`, collision profiles

1. **What it is.** `UWorld::LineTraceTestByChannel(Start, End, Channel, Params)` (`Engine/Classes/Engine/World.h:2128`) returns true if anything blocks the line on that channel. Collision profiles set how a component responds to each channel.
2. **Why it exists.** Visibility, line of fire and ground checks.
3. **How it works.** Synchronous: the physics scene is queried immediately. `FCollisionQueryParams(SCENE_QUERY_STAT(Name), bTraceComplex)` names the query for profiling and chooses simple or per-triangle collision.
4. **When to use it.** A yes/no answer now.
5. **When not to.** Thousands of traces per frame (consider async traces, `01b` §3); when you need the hit point (use `LineTraceSingleByChannel`).
6. **Common mistakes.** Tracing a channel the target ignores; agents blocking their own cover traces (here the `Pawn` profile ignores Visibility, `Config/BaseEngine.ini:3110`).
7. **Example.** `bool bHidden = World.LineTraceTestByChannel(Eye, Chest, ECC_Visibility, Params);`
8. **In this project.** 6,000 traces once at arena build fill the visibility table; blocks and floor use `BlockAll` (`:3104`).

## 15. Core utilities: `FRandomStream`, `FCrc::TypeCrc32`, `TBitArray`, `FParse` / `FCommandLine`, `FPlatformTime`, `FPlatformMisc::RequestExit(WithStatus)`, log macros

1. **What it is.** A seeded random generator (`Core/Public/Math/RandomStream.h:19`), a CRC-32 (`Core/Public/Misc/Crc.h:29`; `TypeCrc32(Value, Crc)` adds one value's bytes, `:36-39`), a packed bit array, command-line parsing, a high-resolution clock, process exit, and `DEFINE_LOG_CATEGORY_STATIC` / `UE_LOG` / `UE_CLOG`.
2. **Why it exists.** Reproducibility (same seed, same arena), proof of reproducibility (a hash), compact tables, run options, timing, and clean exits from automated runs.
3. **How it works.** `FParse::Value(CommandLine, TEXT("SQSeed="), Seed)` leaves `Seed` unchanged if the option is absent; `FParse::Param` tests a flag. `RequestExit(false)` asks the main loop to end; `RequestExitWithStatus(true, Code)` terminates the process with that exit code (`Core/Private/Windows/WindowsPlatformMisc.cpp:1474-1520`).
4. **When to use it.** As above.
5. **When not to.** `FMath::Rand` for anything that must repeat; a forced exit when data still has to be written.
6. **Common mistakes.** Two random draws in one expression (unspecified order, `04` §8); expecting a non-forced exit to return a code.
7. **Example.** `FRandomStream S(7); float X = S.FRandRange(-1.f, 1.f);`
8. **In this project.** The arena layout, its hash, the visibility table, the `-SQSeed=` / `-SQArenaCheck` options, real-time timeouts, and the self-check's exit code (0 on PASS, 1 on FAIL).

## 16. Characters and AI movement: `ACharacter`, `UCharacterMovementComponent`, `AAIController::MoveToLocation` / `StopMovement`, `AutoPossessAI`, `OnRequestFinished`

1. **What it is.** A pawn with a capsule and character movement (walking, gravity, steps), possessed by an AI controller whose path-following component steers it along navmesh paths.
2. **Why it exists.** The standard way to move a humanoid in Unreal, shared by players and AI.
3. **How it works.** `AutoPossessAI = EAutoPossessAI::Spawned` makes the engine spawn `AIControllerClass` for a spawned pawn and possess it. `MoveToLocation(Dest, AcceptanceRadius, bStopOnOverlap, bUsePathfinding, bProjectDestinationToNavigation, bCanStrafe, …)` (`AIModule/Private/AIController.cpp:612`) builds a move request and **finds the path synchronously** (`:899-906`), then path following drives `UCharacterMovementComponent` each frame. It returns `Failed`, `AlreadyAtGoal` or `RequestSuccessful` (`AIModule/Classes/Navigation/PathFollowingComponent.h:154-160`); an accepted request has an id (`GetCurrentMoveRequestID()`, `AIController.h:236`). When the request ends, the path-following component broadcasts `OnRequestFinished(FAIRequestID, const FPathFollowingResult&)` (`PathFollowingComponent.h:223, 229`); the result's flags say whether it succeeded, was interrupted by a newer request or `StopMovement` (`IsInterrupted()`), or ended short of the goal (`Blocked`, `OffPath`, `InvalidPath`). Blocked movement is detected by path following itself: under 10 cm of progress across 10 samples 0.5 s apart (`AIModule/Private/Navigation/PathFollowingComponent.cpp:138-144`). `StopMovement()` (`AIController.h:227`) aborts the move.
4. **When to use it.** Any AI that walks on a navmesh.
5. **When not to.** Thousands of agents (Mass); per-frame re-targeting (every call is a path query).
6. **Common mistakes.** Expecting path following to avoid other agents: it plans around navmesh obstacles only, so agents without avoidance push into each other (turn on `bUseRVOAvoidance`, `CharacterMovementComponent.h:1065`, or use crowd following); calling `MoveToLocation` every frame for the same goal; ignoring how a move ended, so a blocked agent stands still forever; reacting to *every* finished request, including the ones your own new request interrupted; forgetting `bProjectDestinationToNavigation` for goals slightly off the navmesh; leaving `bOrientRotationToMovement` off, so agents slide sideways.
7. **Example.** `Cast<AAIController>(Pawn->GetController())->MoveToLocation(Goal, 50.f, false, true, true, false);`
8. **In this project.** `ASQAgent` (450 cm/s, RVO avoidance, a plain `AAIController`); `USQAgentBrainComponent` is the only caller of `MoveToLocation`, so move requests are counted in one place, and it listens to `OnRequestFinished` to retry a move that ended short of the slot, the same way in every configuration (`05` C3).

## 17. Tickable world subsystems: `UTickableWorldSubsystem`

1. **What it is.** A world subsystem that is also a tickable object: the engine calls its `Tick(float)` every frame.
2. **Why it exists.** World services that need per-frame work without an actor.
3. **How it works.** It ticks in the tickables pass of the frame (after `TG_PostPhysics`, UE5-UpdateBudget `01b` #2). `GetStatId()` names it for the stats system (`RETURN_QUICK_DECLARE_CYCLE_STAT`).
4. **When to use it.** A per-frame service with world lifetime.
5. **When not to.** Work that must run at a chosen tick group (use your own tick function), or work that needs no per-frame update (use timers, `06` §9).
6. **Common mistakes.** Forgetting `GetStatId`; assuming an order between tickables.
7. **Example.** `virtual void Tick(float DeltaTime) override; virtual TStatId GetStatId() const override;`
8. **In this project.** `USQBenchmarkSubsystem` runs the timeline and metrics; the order among tickables doesn't matter here (`02` §6).

## 18. The CSV profiler: `CSV_DEFINE_CATEGORY`, `CSV_EVENT`, `CSV_CUSTOM_STAT`, `CSV_SCOPED_TIMING_STAT`, `CSV_SCOPED_TIMING_STAT_EXCLUSIVE`, `CSV_METADATA`

1. **What it is.** Macros that write one row per frame into a CSV capture: timings, counters, events and metadata (`Core/Public/ProfilingDebugging/CsvProfiler.h`).
2. **Why it exists.** Frame-by-frame numbers from packaged builds, cheap enough to leave on.
3. **How it works.** `CSV_DEFINE_CATEGORY(Name, bOnByDefault)` declares a column group. `CSV_SCOPED_TIMING_STAT(Category, Stat)` times a scope; `CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Stat)` (`CsvProfiler.h:129-131`) records into the `Exclusive` category (on by default, `Core/Private/ProfilingDebugging/CsvProfiler.cpp:73`), and time spent in a nested exclusive scope is not counted twice. `CSV_CUSTOM_STAT(Category, Stat, Value, Op)` (`CsvProfiler.h:155`) records a number. `CSV_EVENT(Category, Text)` marks a frame; the capture can start and stop on events (`-csvStartOnEvent=`, `-csvStopOnEvent=`, parsed at `CsvProfiler.cpp:5081, 5090`). `CSV_METADATA(Key, Value)` (`:85`) is written into the file's last row.
4. **When to use it.** Anything you want as a per-frame number in a benchmark.
5. **When not to.** Per-item stats in hot loops (UE5-UpdateBudget measured about 50 ns per custom stat); Shipping builds (compiled out).
6. **Common mistakes.** Recording a stat per agent instead of once per frame; expecting a category that is off by default to appear; forgetting that **engine code inside your exclusive scope may open its own**: the path query inside `MoveToLocation` records into `Exclusive/GameThread/Pathfinding` (`NS/Private/NavMesh/RecastNavMesh.cpp:3839`) and is subtracted from yours, so a total must add it back.
7. **Example.** `CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SquadQueryDecide);` at the top of a decision function.
8. **In this project.** `SquadQueryDecide` (exclusive) wraps all decision code; AI time = `EnvQueryManager` + `SquadQueryDecide` + `Pathfinding`, summed per frame (`05` C5); `SQBench/Metrics` times the metrics; `SQBench/InCover`, `Awaiting`, `ConflictSlots` are per-frame counts; `BenchStart`, `Threat`, `BenchEnd` are events; run metadata identifies the capture.

## 19. Files and paths: `FFileHelper::SaveStringToFile`, `FPaths::ProjectSavedDir`

1. **What it is.** Write a string to a file; the project's `Saved` folder.
2. **Why it exists.** Platform-independent file output.
3. **How it works.** `SaveStringToFile(Text, Path)` creates missing folders and returns false on failure. In a packaged build `ProjectSavedDir()` is `<build>/SquadQuery/Saved/`.
4. **When to use it.** Small reports after a run.
5. **When not to.** During a measured window (disk I/O can stall a frame).
6. **Common mistakes.** Ignoring the return value; writing inside the capture.
7. **Example.** `FFileHelper::SaveStringToFile(Report, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SQRuns"), Tag + TEXT("_agents.csv")));`
8. **In this project.** The run report is written one frame after `BenchEnd`; failure is an Error line.

## 20. View targets: `ACameraActor`, `APlayerController::SetViewTarget`; class defaults: `GetDefault<T>()`

1. **What it is.** A placed camera, the call that makes a player controller look through it, and read-only access to a class default object.
2. **Why it exists.** A fixed benchmark view without a player pawn; class constants without spawning.
3. **How it works.** The view target is the actor whose camera the player sees. `GetDefault<T>()` returns the class default object.
4. **When to use it.** Fixed or scripted cameras; reading defaults.
5. **When not to.** Changing a class default object at run time.
6. **Common mistakes.** Setting the view target once and losing it when the controller possesses a pawn later: possession re-targets the view while `bAutoManageActiveCameraTarget` is true (`Engine/Classes/GameFramework/PlayerController.h:299`). Calling instance getters on a class default object: its components never registered, so for example `GetSimpleCollisionHalfHeight()` returns 0 on a character's default (`Engine/Private/Character.cpp:234-239`); use `GetDefaultHalfHeight()` (`:263-269`).
7. **Example.** `PC->bAutoManageActiveCameraTarget = false; PC->SetViewTarget(Camera);` and `GetDefault<AMyCharacter>()->GetDefaultHalfHeight()`.
8. **In this project.** A top-down camera set once: the player has no pawn (§21) and auto-management is off (*changed in review*; the first version re-applied it every frame, as UE5-UpdateBudget does). The agent's spawn height comes from `GetDefault<ASQAgent>()->GetDefaultHalfHeight()`.

## 21. Game modes: `AGameModeBase`, `DefaultPawnClass`, `GlobalDefaultGameMode`

1. **What it is.** The game mode is the server-side rules object of a world: it logs players in and gives each a pawn of `DefaultPawnClass` at a `PlayerStart`. `GlobalDefaultGameMode` in `[/Script/EngineSettings.GameMapsSettings]` chooses it for maps that don't override it.
2. **Why it exists.** One place for "what happens when a player joins".
3. **How it works.** `RestartPlayer` spawns the default pawn class at a start spot; if the class is null it skips the spawn and calls `FailedToRestartPlayer`, which only makes the controller inactive (`Engine/Private/GameModeBase.cpp:1292-1305`; `Controller.cpp:115-118`; `PlayerController.cpp:354-359`). `DefaultPawnClass` is a class property, not a config key (`GameModeBase.h:106-108`), so changing it needs a subclass.
4. **When to use it.** Any time the default player setup gets in the way.
5. **When not to.** Per-level rules a designer must change (then a Blueprint game mode or the map's world settings).
6. **Common mistakes.** Expecting `DefaultPawnClass=` in an ini file to work (not config); forgetting that a map's world settings can override the global game mode (check the `LogLoad: Game class is '…'` line).
7. **Example.** `AMyGameMode::AMyGameMode() { DefaultPawnClass = nullptr; }` and `GlobalDefaultGameMode=/Script/MyGame.MyGameMode`.
8. **In this project.** `ASQGameMode` gives the player no pawn, so no `ADefaultPawn` collides with agents at the template map's `PlayerStart` and nothing takes the view target back (`05` C2b).

## 22. EQS from C++: `UEnvQuery`, `UEnvQueryOption`, generators, tests, contexts

1. **What it is.** An EQS query is a `UEnvQuery` holding options (`UEnvQueryOption`), each with one generator (makes candidate items) and a list of tests (filter and/or score them). A context (`UEnvQueryContext`) supplies points or actors the tests measure against.
2. **Why it exists.** Designers author spatial reasoning ("find a hidden spot near me") as data, and the engine runs it time-sliced across all AI.
3. **How it works.** A query can be built in code: `NewObject<UEnvQuery>`, `NewObject<UEnvQueryOption>(Query)`, `Option->Generator = …`, `Option->Tests.Add(…)`, `Query->GetOptionsMutable().Add(Option)` (`AIModule/Classes/EnvironmentQuery/EnvQuery.h:66`). Tests have public settings (`UEnvQueryTest_Trace::TraceData`, `Context`, `TraceFromContext`, `ContextHeightOffset`, `ItemHeightOffset`; `UEnvQueryTest_Distance::TestMode`, `DistanceTo`; `TestPurpose`, `FilterType`, `BoolValue`, `FloatValueMin`, `ScoringEquation` on the base). The manager caches the query **template** per name and run mode, not its results (`01b` #4). A context overrides `ProvideContext(FEnvQueryInstance&, FEnvQueryContextData&)` and fills the data with `UEnvQueryItemType_Point::SetContextHelper`.
4. **When to use it.** Reviewable, fixed benchmark queries; queries generated from game data.
5. **When not to.** Queries designers must tune: author them as assets.
6. **Common mistakes.** Forgetting `TestOrder` (the manager rebuilds the order only if the last test's order is 0, `EnvQueryManager.cpp:874-884`); expecting `SingleResult` to evaluate every item (it moves the most expensive filter last and stops at the first pass, `01b` #3); a context that returns nothing when its input is missing, silently failing every test.
7. **Example.** `UEnvQueryTest_Distance* T = NewObject<UEnvQueryTest_Distance>(Option); T->DistanceTo = UMyContext::StaticClass(); T->TestPurpose = EEnvTestPurpose::Filter; T->FilterType = EEnvTestFilterType::Minimum; T->FloatValueMin.DefaultValue = 800.f;`
8. **In this project.** `SQQueries::MakeAgentCoverQuery` (two filters and a distance score) and, from S5, the squad query; `USquadQueryContext_Threat` gives the threat; `USQCoverSlotGenerator` lists Smart Object slots.

## 23. Running queries: `PrepareQueryInstance`, `RunQuery`, `AbortQuery`, `SilentlyRemoveAllQueriesByQuerier`, EQS debug data

1. **What it is.** The calls that start, stop and inspect queries on the world's `UEnvQueryManager`.
2. **Why it exists.** Queries are owned by a UObject and run over several frames; their owner must be able to cancel them and read what they did.
3. **How it works.** `FEnvQueryRequest(Query, Owner).Execute(Mode, Delegate)` is `PrepareQueryInstance` + `RunQuery` (`EnvQueryManager.cpp:282-286`); calling the two directly keeps the `TSharedPtr<FEnvQueryInstance>`, whose `QueryID` identifies it. The manager steps all queries breadth first within a 10 ms budget per frame (`01b` #2) and calls the finish delegate (`FQueryFinishedSignature`, one `TSharedPtr<FEnvQueryResult>` parameter) when a query completes, fails or is aborted; `AbortQuery(Id)` calls it at once (`:408-428`). `SilentlyRemoveAllQueriesByQuerier(Owner)` drops an owner's queries without calling anything. With `USE_EQS_DEBUGGER` (every build but Test and Shipping, `EnvQueryTypes.h:33`), the instance's `DebugData` records the items each step processed.
4. **When to use it.** Whenever you need the instance after it finishes, or must cancel a query on an event.
5. **When not to.** Fire-and-forget queries: `Execute` is simpler.
6. **Common mistakes.** Creating the finish delegate for every query (one heap allocation each; keep it as a member, as the engine's own Behavior Tree nodes do, `BTTask_RunEQSQuery.cpp:29`); not removing an owner's queries in `EndPlay`; mixing aborted queries into latency figures; forgetting that the **EQS debugger's own bookkeeping runs on every finished query** in Development builds (`01b` #22).
7. **Example.** `auto Instance = Manager->PrepareQueryInstance(FEnvQueryRequest(Query, Owner), EEnvQueryRunMode::SingleResult); Manager->RunQuery(Instance, MyDelegate);`
8. **In this project.** The brain's `StartQuery` / `AbortQuery` / `OnQueryFinished`, and `FSquadQueryWork` reading `DebugData` for item tests.

## 24. Smart Object claims: `CanBeClaimed`, `MarkSlotAsClaimed`, `MarkSlotAsFree`, claim handles

1. **What it is.** Reserving a slot: `MarkSlotAsClaimed(SlotHandle, Priority)` returns an `FSmartObjectClaimHandle` (invalid on failure); `MarkSlotAsFree(ClaimHandle)` releases it; `CanBeClaimed(SlotHandle, Priority)` asks first (`SO/Public/SmartObjectSubsystem.h:622, 631, 684`).
2. **Why it exists.** Exclusive use of interaction points by many agents.
3. **How it works.** A slot is claimable if free, or claimed at a lower priority; the first claim wins (`01b` #7). The EQS generator lists only slots claimable at generation time and the request filter skips claimed slots (`01b` #8, #18), so the claim, made frames later, can fail.
4. **When to use it.** Whenever two agents must not use the same slot.
5. **When not to.** Points nobody reserves.
6. **Common mistakes.** Not releasing a claim when the holder goes away; claiming before freeing your own old slot when swapping; treating a failed claim as an error instead of a race to retry; using the deprecated `Release` alias (`01b` #7).
7. **Example.** `FSmartObjectClaimHandle C = SO->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal); if (!C.IsValid()) { RetryNextFrame(); }`
8. **In this project.** B1 and B2 claim the slot their query picked and retry next frame on failure; from S5 the squad subsystem claims for its members.

## 25. Tick intervals: `SetComponentTickEnabled`, `SetComponentTickInterval`, `SetComponentTickIntervalAndCooldown`

1. **What it is.** A component's tick can be enabled and given an interval (seconds between ticks) instead of running every frame.
2. **Why it exists.** Periodic AI (a Behavior Tree service, a poll) without a per-frame cost.
3. **How it works.** An enabled tick function joins the enabled list and **runs on the next frame, whatever its interval** (`Engine/Private/TickTaskManager.cpp:1681-1691`); after each tick it is rescheduled with its current `TickInterval`. `SetComponentTickIntervalAndCooldown` reschedules an already-interval tick, but does nothing to the cooldown before the first tick (`:2463-2477`, `bWasInterval`).
4. **When to use it.** Anything that should run every N seconds.
5. **When not to.** Work that must happen at a fixed point in the frame relative to other ticks (use tick groups and prerequisites).
6. **Common mistakes.** Expecting a cooldown set before the first tick to delay that tick, so many agents tick in step on their first frame; changing the interval and expecting it to apply to the tick already scheduled.
7. **Example.** In the first `TickComponent`: `SetComponentTickInterval(SeededDelay); return;`, then the normal interval from the next tick on.
8. **In this project.** B0 and B1 poll every 0.5 s ± 0.1 s with a seeded first delay applied by the first tick (`05` D6).

## 26. `TWeakObjectPtr` and `TOptional`

1. **What it is.** `TWeakObjectPtr<T>`: a pointer to a UObject that becomes null when the object is destroyed, without keeping it alive. `TOptional<T>`: a value that may be absent.
2. **Why it exists.** Caching another object's pointer without owning it; "not set yet" without a magic value.
3. **How it works.** `Weak.Get()` returns the object or null; `Optional.IsSet()`, `GetValue()`, `Reset()`.
4. **When to use it.** A cache of something another system owns; a one-time pending value.
5. **When not to.** An object you must keep alive (`UPROPERTY` `TObjectPtr`); hot loops that call `Get()` many times (resolve once into a local).
6. **Common mistakes.** Storing a raw pointer to another object's UObject (dangles after destruction); a separate `bool` flag next to a value that means "value set" (use `TOptional`).
7. **Example.** `TWeakObjectPtr<const USquad> Squad; if (const USquad* S = Squad.Get()) { … }`
8. **In this project.** The brain caches its squad (`TWeakObjectPtr<const USquad>`) and its pending first-poll delay (`TOptional<float>`).

## 27. Automation tests: `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `EAutomationTestFlags`, `TestEqual`, `AddInfo`

1. **What it is.** The engine's unit-test framework: a macro declares a test class with a dotted name and flags, and its `RunTest` reports results through `TestEqual`, `TestTrue`, `TestNotEqual`, `AddInfo`, `AddError`.
2. **Why it exists.** Tests that run headless in the editor or a game, from the command line or the Session Frontend.
3. **How it works.** `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClass, "Group.Name", Flags)` (`Core/Public/Misc/AutomationTest.h`) registers the test. The flags combine contexts (`EditorContext`, `ClientContext`, …) with exactly one filter (`ProductFilter`, `EngineFilter`, …). `UnrealEditor-Cmd Project.uproject -ExecCmds="Automation RunTests Group;Quit" -unattended -nullrhi` runs every test whose name starts with `Group` and exits non-zero if one fails. Test code sits inside `#if WITH_DEV_AUTOMATION_TESTS`.
4. **When to use it.** Pure logic (a solver, a parser) and small world-level checks.
5. **When not to.** Performance numbers: the editor is not the shipped build (report timings with `AddInfo` as orders of magnitude only).
6. **Common mistakes.** Two filter flags, or none; asserting inside a 1,000-iteration loop (1,000 error lines); a "determinism" test that compares two calls in one process (it proves nothing; pin the expected answer).
7. **Example.** `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMyTest, "MyGame.Solver.Basics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter) bool FMyTest::RunTest(const FString&) { TestEqual(TEXT("sum"), 1 + 1, 2); return true; }`
8. **In this project.** `SquadQuery.Solver.*` (`05` E3), run by `Tools\run_tests.bat`.

## 28. Views and checks: `TConstArrayView`, `ensureMsgf`

1. **What it is.** `TConstArrayView<T>`: a pointer and a length over someone else's contiguous elements, read-only. `ensureMsgf(Condition, Format, …)`: report a broken assumption (once per call site, with a callstack in the log) and carry on.
2. **Why it exists.** Functions that read arrays without caring how they were stored; recoverable caller bugs that must not crash.
3. **How it works.** A view converts implicitly from a `TArray`, an initializer list or a C array; `Num()`, `GetData()`, `ContainsByPredicate`. `ensureMsgf` returns the condition, so it can guard an early return.
4. **When to use it.** Read-only array parameters; input validation at a library boundary.
5. **When not to.** Storing a view beyond the caller's array lifetime; `ensure` for conditions that are normal at run time (it logs an error).
6. **Common mistakes.** Keeping a view to a temporary; computing `rows × columns` in 32 bits for a size check.
7. **Example.** `if (!ensureMsgf(int64(Cost.Num()) == int64(N) * M, TEXT("bad size"))) { return 0.f; }`
8. **In this project.** `FSquadAssignmentSolver::Solve` takes the cost matrix as a view and ensures its size (`05` E2).

## 29. `TInlineAllocator`

1. **What it is.** An allocator for `TArray` that keeps the first N elements inside the array object itself (on the stack, for a local array) and moves to the heap only beyond N.
2. **Why it exists.** Small, short-lived arrays without a heap allocation.
3. **How it works.** `TArray<double, TInlineAllocator<160>> A;` — the array object is 160 doubles larger; `Init`, `Add` and indexing work as usual.
4. **When to use it.** Per-call scratch arrays of a predictable size on a hot path.
5. **When not to.** Large N on deep call stacks; arrays stored in many objects (every object pays for the inline storage).
6. **Common mistakes.** An inline size far larger than ever needed (stack waste) or far smaller (no benefit).
7. **Example.** `TArray<int32, TInlineAllocator<16>> Members;`
8. **In this project.** The solver's six scratch arrays (`05` E2); the squad's member lists in S5.
