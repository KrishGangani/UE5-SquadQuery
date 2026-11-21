# 05 · Line By Line Code Explanation: SquadQuery

**Rule:** every file that exists in `SquadQuery\` (and every script in `Tools\`) is explained here, line by line, in the same step it is written. Sections follow the build order in [03_Implementation_Roadmap.md](03_Implementation_Roadmap.md).

| Part | Step | Files |
|---|---|---|
| A | Phase 4, F1–F3 (foundation) | `.uproject`, two `Target.cs`, two `Build.cs`, two module `.cpp`, `.uplugin`, three config `.ini`, `Tools\*.bat` |
| B | S1 (arena and navigation) | `SQCoverBlock.h/.cpp`, `SQArenaSubsystem.h/.cpp`, the changed `SquadQuery.Build.cs` |
| C | S2 (agents, director, scripts) | `SQAgent.h/.cpp`, `SQAgentBrainComponent.h/.cpp` (S2 version), `SQBenchmarkSubsystem.h/.cpp`, the changed `SquadQuery.Build.cs` and `DefaultEngine.ini`, `Tools\csv_stats.py`, `Tools\run_bench.py` |
| D | S3 (squads, the per-agent query, baselines B0-B2) | `SquadQueryCore`: `Squad.h`, `SquadMemberComponent.h/.cpp`, `SquadQuerySubsystem.h/.cpp`, `SquadQueryContext_Threat.h/.cpp`, `SquadQueryWork.h/.cpp`, `Build.cs`; game: `SQCoverSlotGenerator.h/.cpp`, `SQQueries.h/.cpp`, `SQAgentBrainComponent` (S3 version), director, agent and arena changes; `Tools\run_bench.py`, `Tools\analyse_events.py` |
| E | S4 onwards | written with the code |

Engine references are relative to `C:\Program Files\Epic Games\UE_5.8\Engine\`, version 5.8.4.

---

## Part A · Foundation (Phase 4)

There is no gameplay code yet. Part A is the *skeleton* every later step builds on: what tells the engine "this is a project", which executables to build, which modules exist, how they depend on each other, and which engine settings the benchmark relies on.

### A1. `SquadQuery.uproject`: the project descriptor

```json
{
	"FileVersion": 3,
	"EngineAssociation": "5.8",
	"Category": "",
	"Description": "P07 SquadQuery: shared tactical decisions for squads (UE 5.8.4 portfolio project).",
	"Modules": [ { "Name": "SquadQuery", "Type": "Runtime", "LoadingPhase": "Default" } ],
	"Plugins": [
		{ "Name": "SquadQueryCore", "Enabled": true },
		{ "Name": "SmartObjects", "Enabled": true },
		{ "Name": "AndroidFileServer", "Enabled": false }
	]
}
```

| Line | What it does, and why |
|---|---|
| `"FileVersion": 3` | The descriptor schema version. Epic's own 5.8 `TP_Blank` template uses 3 (`Templates/TP_Blank/TP_Blank.uproject`, one folder above `Engine\`). A wrong value makes the editor refuse or try to upgrade the file |
| `"EngineAssociation": "5.8"` | Which installed engine opens the project. A launcher build is identified by its version string. The template leaves it blank, and double-clicking such a file asks which engine to use |
| `"Category"`, `"Description"` | Shown in the project browser. Cosmetic |
| `"Modules"` | The C++ modules the **project** owns. UnrealBuildTool (UBT) compiles exactly these, plus the modules of enabled plugins |
| `"Name": "SquadQuery"` | Must match the folder `Source\SquadQuery\` and the class in `SquadQuery.Build.cs`. Module names must be unique: UBT keys `.Build.cs` files by name in a case-insensitive dictionary and skips a second module with the same name (`Source/Programs/UnrealBuildTool/Configuration/Rules/RulesAssembly.cs:199-206`). That is why the plugin is `SquadQueryCore`, not `SquadQuery` |
| `"Type": "Runtime"` | Built into game **and** editor targets. *Alternative:* `Editor` (editor only), which would break the packaged game |
| `"LoadingPhase": "Default"` | Loaded at the normal point, after engine start-up and before the first world exists, so the module's world subsystems are registered before the map loads |
| `"SquadQueryCore", "Enabled": true` | Enables the project's own plugin. It is `EnabledByDefault` anyway, but **UBT requires the project to list every plugin whose modules the game module depends on**: otherwise it warns "*Project* does not list plugin '…' as a dependency, but module '…' depends on '…'" (`Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:1660-1690`). The game module depends on `SquadQueryCore`, so the entry keeps the build at 0 warnings |
| `"SmartObjects", "Enabled": true` | The SmartObjects plugin is **off by default** (`Plugins/Runtime/SmartObjects/SmartObjects.uplugin:13`). Cover slots are Smart Object slots, and the game module will depend on `SmartObjectsModule` from S1, so it is enabled now. Enabling it in Phase 4 also proves early that a launcher engine can build, cook and run it (`09` F-1 to F-6) |
| `"AndroidFileServer", "Enabled": false` | **Switches off an engine plugin that is on by default** (`Plugins/Runtime/AndroidFileServer/AndroidFileServer.uplugin:15`). Its editor module, the first time its settings load with the placeholder token `[AUTO]`, generates a random GUID security token and saves its whole settings section into the project's `DefaultEngine.ini` (`Plugins/Runtime/AndroidFileServer/Source/AndroidFileServerEditor/Private/AndroidFileServerRuntimeSettings.cpp:27-37`). The first Phase 4 cook did exactly that (`09` observation 1). A generated token must not reach a public repository, and the project never targets Android, so the plugin is disabled and the section deleted (portfolio rule, 2026-11-21). Re-verified by the Phase 4 re-run (`09` R-1 to R-7) |

**Not listed on purpose:** StateTree and GameplayStateTree (only the optional step S8 needs them; listing them now would load modules nothing uses), and EnvironmentQueryEditor (an editor plugin for authoring EQS assets; our queries are built in code).

### A2. `Source\SquadQuery.Target.cs` and `SquadQueryEditor.Target.cs`: build targets

```csharp
using UnrealBuildTool;

public class SquadQueryTarget : TargetRules
{
	public SquadQueryTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("SquadQuery");
	}
}
```

| Line | What it does, and why |
|---|---|
| `using UnrealBuildTool;` | Target files are **C#**, compiled by UBT itself, not by the C++ compiler. This brings `TargetRules`, `TargetType` and the rest into scope |
| `public class SquadQueryTarget : TargetRules` | UBT finds targets by class name, `<Name>Target`. `SquadQueryTarget` defines the target `SquadQuery`, which is why `build.bat SquadQuery …` works |
| `(TargetInfo Target) : base(Target)` | UBT passes in the platform, configuration and architecture being built; the base constructor stores them |
| `Type = TargetType.Game` | A standalone game executable with no editor code: the one that gets packaged. The editor file uses `TargetType.Editor`, which builds DLLs loaded by `UnrealEditor.exe` |
| `DefaultBuildSettings = BuildSettingsVersion.V7` | The newest default compiler and build settings for this engine. **V7 is what Epic's 5.8 template uses** (`Templates/TP_Blank/Source/TP_Blank.Target.cs`) |
| `IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8` | 5.8's engine header include order (Epic trims transitive includes each version). Pinning it means an engine upgrade can't silently change what our code includes |
| `ExtraModuleNames.Add("SquadQuery")` | Which project module to build. Plugin modules come in through the plugin system |

**Why two target files?** The editor and the packaged game are different executables with different module sets, and UBT needs one target rule per executable type. The editor file differs only in the class name (`SquadQueryEditorTarget`) and `Type = TargetType.Editor`.

### A3. `Source\SquadQuery\SquadQuery.Build.cs`: the game module's build rules

```csharp
public class SquadQuery : ModuleRules
{
	public SquadQuery(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "SquadQueryCore" });
	}
}
```

| Line | What it does, and why |
|---|---|
| `public class SquadQuery : ModuleRules` | The module's build rules. The class name must equal the module name |
| `(ReadOnlyTargetRules Target)` | The target being built, read-only: a module can *read* the target's settings (for example `Target.Configuration`) but can't change them |
| `PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs` | Use a shared precompiled header built for a common set of engine modules, unless the module names its own. Fast builds with no PCH file to maintain. It has been the default since `BuildSettingsVersion.V2` (`Source/Programs/UnrealBuildTool/Configuration/Rules/TargetRules.cs:131-135`; getter in `Rules/ModuleRules.cs:816-830`); stating it documents the choice |
| `PublicDependencyModuleNames … "Core", "CoreUObject", "Engine"` | The engine basics. Public, because the module's public headers (from S1) will expose engine types such as `UWorldSubsystem` and `ACharacter` |
| `PrivateDependencyModuleNames … "SquadQueryCore"` | The game module uses the plugin, never the reverse (`02` §3). **Private for now** because no public header of the game module includes a plugin header yet. It becomes public in the first step where one does (S3: `USQAgentBrainComponent.h` refers to the member component). A public dependency also hands the plugin's include paths to every module that depends on us, so the narrower choice is the default |

**Why not add `AIModule`, `NavigationSystem` and `SmartObjectsModule` now?** Every dependency costs build time and couples modules. Each is added in the step that first includes its headers (S1 and S3), and its explanation is written then.

### A4. `Source\SquadQuery\Private\SquadQueryModule.cpp`: the game module's entry point

```cpp
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, SquadQuery, "SquadQuery");
```

| Line | What it does, and why |
|---|---|
| `#include "Modules/ModuleManager.h"` | Declares the module macros and the default module classes (`Source/Runtime/Core/Public/Modules/ModuleManager.h`) |
| `IMPLEMENT_PRIMARY_GAME_MODULE(…)` | Generates the module's entry point and marks it as the project's **primary** game module, which sets up project-wide globals in packaged builds (`ModuleManager.h:1100-1136`). Exactly one module per project uses the primary variant |
| `FDefaultGameModuleImpl` | An empty module class (`ModuleManager.h:892`): no `StartupModule`/`ShutdownModule` logic is needed, because everything in this project is a subsystem or an actor that the engine creates by itself |
| `SquadQuery` | The module name; must match `Build.cs` and the `.uproject` |
| `"SquadQuery"` | The game name argument. It no longer sets anything: in monolithic (packaged) builds the parameter is named `DEPRECATED_GameName` (`ModuleManager.h:1100, 1114`), and in modular (editor) builds "the game name will be set via the command-line" (`:1130-1131`). It is passed only because the macro still takes three arguments |

The file is in `Private\` because nothing else includes it.

### A5. `Plugins\SquadQueryCore\SquadQueryCore.uplugin`: the plugin descriptor

```json
{
	"FileVersion": 3, "Version": 1, "VersionName": "1.0",
	"FriendlyName": "Squad Query Core",
	"Description": "Evaluates the environment once per squad with EQS, …",
	"Category": "AI",
	"EnabledByDefault": true, "CanContainContent": false,
	"IsBetaVersion": false, "IsExperimentalVersion": false, "Installed": false,
	"Modules": [ { "Name": "SquadQueryCore", "Type": "Runtime", "LoadingPhase": "Default" } ],
	"Plugins": [ { "Name": "SmartObjects", "Enabled": true } ]
}
```

| Field | Value | Why |
|---|---|---|
| `FileVersion` | 3 | The plugin descriptor schema the 5.8 engine writes |
| `Version`, `VersionName` | 1, "1.0" | Our own version numbers. Only used for display and for dependency checks by other plugins |
| `FriendlyName`, `Description`, `Category` | text | Shown in the editor's Plugins window. The description says what the system does, in one sentence |
| `CreatedBy…`, `DocsURL`, `MarketplaceURL`, `SupportURL` | empty | Optional fields, left empty |
| `EnabledByDefault` | true | Any project that contains the plugin gets it, without an extra step |
| `CanContainContent` | false | The plugin is code only. No `Content\` folder is mounted, which keeps cooking simpler |
| `IsBetaVersion`, `IsExperimentalVersion` | false, false | No warning badge. A statement about API stability inside this project, **not** a claim of production readiness |
| `Installed` | false | Set to true only for plugins installed into an engine. Ours lives in the project |
| `Modules[0]` | `SquadQueryCore`, `Runtime`, `Default` | One runtime module, loaded at the default phase. The SmartObjects module loads earlier (`PreDefault`, `SmartObjects.uplugin`), so it is available when ours loads; the order of the phases is `PreDefault` → `Default` → `PostDefault` (`Source/Runtime/Projects/Public/ModuleDescriptor.h:44-50`) |
| `"Plugins": [SmartObjects]` | enabled | **Declares that this plugin depends on SmartObjects.** From S3/S5 the core module links against `SmartObjectsModule` (claims). Without this entry UBT warns "Plugin '…' does not list plugin 'SmartObjects' as a dependency, but module '…' depends on module '…'" (`UEBuildTarget.cs:1560-1580`), and the build must have 0 warnings. It also means any project that enables SquadQueryCore gets SmartObjects enabled automatically. It is declared now, before the module uses it, because it is part of the architecture (`02` §3) and costs nothing |

### A6. `SquadQueryCore.Build.cs` and `SquadQueryCoreModule.cpp`: the plugin module

```csharp
public class SquadQueryCore : ModuleRules
{
	public SquadQueryCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
```

```cpp
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, SquadQueryCore);
```

| Line | What it does, and why |
|---|---|
| `PublicDependencyModuleNames … "Core", "CoreUObject", "Engine"` | Public, because the plugin's public headers will expose engine types to the game module (`UTickableWorldSubsystem`, `UActorComponent`, `UEnvQueryContext`). `AIModule`, `SmartObjectsModule`, `GameplayTags` and `DeveloperSettings` are added in the steps that first include them (S3–S6) |
| `IMPLEMENT_MODULE(FDefaultModuleImpl, SquadQueryCore)` | The plugin module's entry point (`ModuleManager.h:946-955`). A plugin module uses `IMPLEMENT_MODULE`, never the primary game variant. `FDefaultModuleImpl` (`ModuleManager.h:884`) because the module needs no start-up logic |

### A7. `Config\DefaultEngine.ini`

| Section / key | Value | Why |
|---|---|---|
| `GameMapsSettings` `GameDefaultMap`, `EditorStartupMap` | `/Engine/Maps/Templates/Template_Default` | An engine map with a floor and lights, so no map has to be authored. Everything the benchmark needs is spawned by code from a seed (`02` §4.2). The same choice worked in UE5-UpdateBudget's packaged builds on this machine |
| `[/Script/Engine.Engine]` `bSmoothFrameRate=False`, `bUseFixedFrameRate=False` | restated defaults | No smoothing or cap may hide real frame cost. Restated so a later change can't alter a benchmark condition silently |
| `[SystemSettings]` `r.VSync=0` | off | Present never blocks the frame |
| `[/Script/Engine.RendererSettings]` `r.AllowStaticLighting=False`, `r.Shadow.Virtual.Enable=1` | as UE5-UpdateBudget | Everything is spawned at runtime, so baked lighting is impossible anyway. Same rendering settings as the reference project, so GPU behaviour is comparable |
| `[/Script/WindowsTargetPlatform.WindowsTargetSettings]` | DX12, SM6 only | The RHI and shader model that UE5-UpdateBudget verified on this machine's RTX GPU. `-…SM5` removes the SM5 entry the engine adds by default (`Config/BaseEngine.ini:3412`), so only one shader format is cooked |
| `[/Script/NavigationSystem.NavigationSystemV1]` `bAutoCreateNavigationData=True` | restated default (`Config/BaseEngine.ini:3038-3039`) | The navigation system spawns the `RecastNavMesh` itself when the runtime bounds volume appears (`01b` #9). Restated because the whole arena depends on it |
| `[/Script/NavigationSystem.RecastNavMesh]` `RuntimeGeneration=Dynamic` | **not** a default | `BaseEngine.ini` sets `Static` (`:3042-3043`). Our cover blocks and nav bounds are spawned at runtime, so the navmesh must be generated at runtime (`Source/Runtime/NavigationSystem/Public/NavigationData.h:523-533`). `RecastNavMesh` is `config=Engine` (`NavMesh/RecastNavMesh.h:570`), so this section applies to it. **Verified** in Phase 4: `getall RecastNavMesh RuntimeGeneration showdefaults` prints `Dynamic` in both the uncooked and the packaged game (`09` R-4, R-7). Building a real navmesh is checked in S1 |
| *(no Android File Server section)* | **deliberately absent** | The first cook wrote an `[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]` section with a generated security token into this file (`09` observation 1). The section is deleted, and the plugin that writes it is disabled in the `.uproject` (A1), so it can't come back. The Phase 4 re-run checks that the file stays unchanged after a build, a cook and two game runs (`09` R-8) |

### A8. `Config\DefaultGame.ini`

| Section / key | Value | Why |
|---|---|---|
| `GeneralProjectSettings` `ProjectName`, `Description` | text | Shown in window titles and the project browser |
| `ProjectPackagingSettings` `+DirectoriesToAlwaysCook=(Path="/Engine/BasicShapes")` | the basic shapes | Cover blocks and agents use the engine cube and cylinder, referenced only from C++. No map references them, so the cooker would leave them out and the packaged build would show nothing |
| `[/Script/AIModule.EnvQueryManager]` `MaxAllowedTestingTime=0.01`, `bTestQueriesUsingBreadth=true` | **restated defaults** (`Config/BaseGame.ini:230-232`) | The EQS time slice is a measured condition of every configuration (`01b` #2). The manager is `config=Game` (`AIModule/Classes/EnvironmentQuery/EnvQueryManager.h:206`), so its section belongs in `DefaultGame.ini`, not `DefaultEngine.ini` (`07`) |

### A9. `Config\DefaultInput.ini`

```ini
[/Script/Engine.InputSettings]
DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput
DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent
```

| Key | Why |
|---|---|
| `DefaultPlayerInputClass` | The engine's own default is the legacy `UPlayerInput` (`Source/Runtime/Engine/Private/UserInterface/InputSettings.cpp:52`). The Enhanced Input plugin is enabled by default (`Plugins/EnhancedInput/EnhancedInput.uplugin:13`), casts the player's input object to `UEnhancedPlayerInput` (`Plugins/EnhancedInput/Source/EnhancedInput/Private/EnhancedInputSubsystems.cpp:79`), and logs `LogEnhancedInput: Warning … does not have a valid PlayerInput object` on every map load when the cast fails (`:118`). Other portfolio projects hit exactly that warning in their first Phase 4 runs, so this project ships the fix from the start. These are the values Epic's 5.8 templates use |
| `DefaultInputComponentClass` | The matching input component class (`InputSettings.cpp:53` is the legacy default). The game has no player input yet; the pair keeps the startup log clean |

### A10. `Tools\build.bat`, `package.bat`, `smoke_game.bat`

All three are thin wrappers, so every build and run uses exactly the same command line, and the queue (`tools\ue_queue.py`, outside this repository) can run them. They are stored with CRLF line endings (`.gitattributes`), because `goto` and labels are a known pitfall in LF-only batch files (`smoke_game.bat` uses a label) **[unverified on this machine; avoided rather than tested]**.

**`build.bat [Target] [Configuration]`**

| Line | What it does, and why |
|---|---|
| `@echo off`, `setlocal` | No command echo; variables stay local to the script |
| `set "UE_ROOT=…UE_5.8"` | The one place that names the engine install. Quoted with the `set "NAME=value"` form so the spaces in the path are safe |
| `set "PROJECT=%~dp0..\SquadQuery\SquadQuery.uproject"` | `%~dp0` is the folder of the script itself, so the script works from any current directory |
| `if "%TARGET%"=="" set "TARGET=SquadQueryEditor"` and the same for `CONFIG` | Defaults: the editor target, Development |
| `call "…\Build.bat" %TARGET% Win64 %CONFIG% -Project="%PROJECT%" -WaitMutex -NoHotReloadFromIDE` | The engine's own build entry point. `-WaitMutex` waits if another UBT is running instead of failing; `-NoHotReloadFromIDE` avoids the hot-reload path, since an editor is never open during these builds |
| `exit /b %ERRORLEVEL%` | Passes UBT's exit code to the queue, so a failed build fails the job |

**`package.bat [Configuration]`**: `RunUAT.bat BuildCookRun -project=… -noP4 -utf8output -unattended -platform=Win64 -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory=…\Build`. Build the game target, cook content, stage, pack into `.pak` files and copy the result to `Build\Windows\` (git-ignored). `-noP4`: no Perforce. `-unattended`: no dialogs. `-clientconfig=Development`: the headline benchmark build, because the launcher engine can't build Test (UE5-UpdateBudget `09` F-3a). The flags were verified by UE5-UpdateBudget's Phase 2 (`Source/Programs/AutomationTool`, its `01b` §4).

**`smoke_game.bat [packaged | extra args]`**

| Mode | Command | Purpose |
|---|---|---|
| (default) uncooked | `UnrealEditor-Cmd.exe "<uproject>" -game -nullrhi -nosound -unattended -nosplash -traceautostart=0 -ExecCmds="quit" -log -abslog=…\SquadQuery\Saved\Logs\SmokeGame.log` | Runs the game from the editor binary without cooking or a GPU: proves that the modules and plugins load and the map opens. `-ExecCmds="quit"` quits as soon as the map is loaded; `-abslog` puts the log at a fixed path (`Saved\` is git-ignored) |
| `packaged` | `Build\Windows\SquadQuery.exe -windowed -ResX=1280 -ResY=720 -nosound -unattended -traceautostart=0 -ExecCmds="quit" -log -abslog=…\Build\SmokePackaged.log` | Runs the cooked build on the real GPU at the benchmark resolution: proves the packaged game, the cooked SmartObjects plugin and the RHI work |

`-traceautostart=0` stops an open Unreal Insights from auto-tracing the run (`Source/Runtime/Core/Private/ProfilingDebugging/TraceAuxiliary.cpp:267, 2378`). `-ExitAfterSeconds` is **not** passed: 5.8.4 doesn't parse it (no match in `Source/Runtime`), so it would do nothing; `-ExecCmds="quit"` ends the run. A smoke run is not timed, so it is not pinned to the performance cores; the benchmark script (S2) pins itself to them and launches the real executable `Build\Windows\SquadQuery\Binaries\Win64\SquadQuery.exe`, which inherits the mask (`01` §7).

### A11. What is deliberately not here yet

- No `Content\` assets and no map: the arena is spawned by code (S1).
- No C++ classes: Phase 4 is the skeleton only (Prompt 7, Phase 4 "folder structure, classes, and modules"; the classes are designed in `02` §4 and written step by step from S1).
- No benchmark scripts (`csv_stats.py`, `run_bench.py`): they are copied and adapted from UE5-UpdateBudget in S2, when there is something to measure.

---

## Part B · S1: the arena, its Smart Object slots and its runtime navmesh

S1 builds the test arena of `01` §6.1 and proves, in a **cooked** build, the two assumptions everything else rests on: that a navmesh can be generated for geometry spawned at runtime, and that Smart Objects can be created at runtime without assets (`01b` #9, #11). Two classes, both in the game module, because the arena is benchmark content, not part of the reusable system (`02` §3).

### B1. `SquadQuery.Build.cs` (changed in S1)

| Line | What changed, and why |
|---|---|
| `PublicDependencyModuleNames … "SmartObjectsModule"` | **Public** now, because two *public* headers need it: `SQCoverBlock.h` derives a class from `USmartObjectBehaviorDefinition`, and `SQArenaSubsystem.h` stores `FSmartObjectSlotHandle`. Any module that includes those headers needs the Smart Object include paths, which is exactly what "public" passes on (`06` §2). The `.uproject` already lists the SmartObjects plugin, so UBT doesn't warn (`05` A1) |
| `PrivateDependencyModuleNames … "NavigationSystem"` | Only `SQArenaSubsystem.cpp` uses navigation (`UNavigationSystemV1`, `ANavMeshBoundsVolume`, `UNavigationPath`). Private keeps those include paths away from other modules. NavigationSystem is an engine module, not a plugin, so nothing has to be declared in the `.uproject` |
| `PrivateDependencyModuleNames … "GameplayTags"` | *Added after the second S1 build failed to link* (`09` S1-b). The Smart Object request filter and slot definition contain gameplay tag containers and queries, and `SQArenaSubsystem.cpp` instantiates their constructors and copies, so this module links against `GameplayTags` itself. `SmartObjectsModule` lists `GameplayTags` as a public dependency, yet the link still failed until it was added here **[observed]**: depend directly on every module whose types your own code constructs or copies (`07` S1) |
| `SquadQueryCore` stays private | No public header of the game module includes a core header yet. That changes in S3 |

`AIModule` is still not added: S1 uses no EQS and no AI controller.

### B2. `Public\SQCoverBlock.h`

```cpp
UCLASS(HideDropdown, NotBlueprintType)
class USQCoverBehaviorDefinition : public USmartObjectBehaviorDefinition { GENERATED_BODY() };

UCLASS()
class ASQCoverBlock : public AActor
{
	GENERATED_BODY()
public:
	ASQCoverBlock();
	void SetCoverDefinition(USmartObjectDefinition* Definition);
	USmartObjectComponent* GetSmartObjectComponent() const { return SmartObject; }
	static constexpr float LengthCm = 300.f;     // and ThicknessCm = 100.f, HeightCm = 130.f
private:
	UPROPERTY(VisibleAnywhere, Category = "Cover") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Cover") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Cover") TObjectPtr<USmartObjectComponent> SmartObject;
};
```

| Line | What it does, and why |
|---|---|
| `#include "GameFramework/Actor.h"`, `"SmartObjectDefinition.h"` | The two base classes. `SmartObjectDefinition.h` declares `USmartObjectBehaviorDefinition` (`SO/Public/SmartObjectDefinition.h:65-69`); a base class must be complete, so a forward declaration isn't enough |
| `#include "SQCoverBlock.generated.h"` | The code UnrealHeaderTool (UHT) writes for the two `UCLASS`es. It must be the **last** include of a reflected header (`06` §6) |
| `class USmartObjectComponent; class UStaticMeshComponent;` | Forward declarations: the header only stores pointers to them, so their full definitions are needed only in the `.cpp`. Every file that includes this header compiles faster |
| `UCLASS(HideDropdown, NotBlueprintType)` | `HideDropdown` keeps the class out of editor class pickers; `NotBlueprintType` keeps it out of Blueprint variable types. It is an implementation detail of the arena. The same specifiers as Epic's test behavior (`SmartObjectsTestSuite/Public/SmartObjectTestTypes.h`) |
| `class USQCoverBehaviorDefinition : public USmartObjectBehaviorDefinition` | **Why it exists:** a Smart Object definition only passes validation if every slot carries a behavior definition (or the definition has a default one), and an invalid definition stops every component from registering (`01b` #19). It carries no data; its type says "this slot is cover" |
| `GENERATED_BODY()` | UHT's per-class boilerplate: reflection data, `StaticClass()`, the constructors the object system needs (`06` §6) |
| `UCLASS() class ASQCoverBlock : public AActor` | One cover block. An actor, because it has a place in the world, collision and components. No `_API` export macro: nothing outside this module uses it, the same convention as UE5-UpdateBudget's game module |
| `ASQCoverBlock();` | The constructor builds the components. Unreal runs it once for the **class default object** (CDO), and every spawned instance is initialised from it |
| `void SetCoverDefinition(USmartObjectDefinition*)` | Must be called between `SpawnActorDeferred` and `FinishSpawning`: in game worlds the Smart Object component registers in `BeginPlay`, and once registered it refuses a new definition (`SO/Private/SmartObjectComponent.cpp:296-300, 359-372`) |
| `GetSmartObjectComponent() const` | The arena needs the component's registered handle to look up its slots. `const`: it doesn't change the block |
| `static constexpr float LengthCm = 300.f;` (and `ThicknessCm`, `HeightCm`) | The block size, in one place. `static`: one value for the class, not per object. `constexpr`: a compile-time constant, so the arena's slot offsets are computed from it at compile time (`04` §5). `float` because component scales and locations take floats |
| `static constexpr const TCHAR* CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");` and `static constexpr float CubeSizeCm = 100.f;` | *Added in review:* the engine cube that both the blocks and the arena floor are made of, and its edge length. The path and the "divide by 100" were each written twice before (block and floor); now one constant each. A `constexpr` pointer to a string literal is a compile-time constant too (`04` §5) |
| `UPROPERTY(VisibleAnywhere, Category = "Cover") TObjectPtr<…>` (three) | Each component pointer is a `UPROPERTY`, so the garbage collector sees the reference and the editor can show it. `VisibleAnywhere`: visible but not replaceable in the details panel, the usual choice for components made in the constructor. `TObjectPtr` is the UE5 form of a UObject member pointer (`06` §7) |
| `Root` (`USceneComponent`) | An **unscaled** root. Smart Object slot offsets are relative to the actor's transform, so a scaled root would scale the offsets too |
| `Mesh` (`UStaticMeshComponent`) | The visible, colliding block: the engine cube, scaled on the component, not on the actor |
| `SmartObject` (`USmartObjectComponent`) | Owns the block's six cover slots in the Smart Object runtime |

### B3. `Private\SQCoverBlock.cpp`

| Line | What it does, and why |
|---|---|
| The includes | Full definitions of what the constructor uses ("include what you use"), so the file stays correct when other headers stop including them for us |
| `PrimaryActorTick.bCanEverTick = false;` | A block never ticks. Without this line every block would register a tick function that does nothing; 100 of them would cost tick dispatch every frame (the overhead UE5-UpdateBudget measured) |
| `Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));` | Creates the component as a **default subobject**: part of the class template, created in every instance, named "Root". Only valid in a constructor |
| `Root->SetMobility(EComponentMobility::Static);` (and on `Mesh`) | The block never moves. Navigation treats it as fixed geometry and the renderer can cache it. Set before registration, which is allowed for objects spawned at runtime |
| `SetRootComponent(Root);` | `Root` defines the actor's transform |
| `static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(CubeMeshPath);` | Finds the engine cube. `static`: the lookup runs once for the class, not once per block. `FObjectFinder` only works in constructors. `CubeMeshPath` is a class constant (B2) that the arena's floor uses too, so the path is written once. The asset is cooked because `DefaultGame.ini` always cooks `/Engine/BasicShapes` (`05` A8) |
| `Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); Mesh->SetupAttachment(Root);` | The visible block under the root. `SetupAttachment` is the constructor-time form of attaching (nothing is registered yet) |
| `Mesh->SetStaticMesh(CubeMesh.Object);` | Uses the found cube |
| `Mesh->SetRelativeScale3D(FVector(LengthCm, ThicknessCm, HeightCm) / CubeSizeCm);` | The engine cube is `CubeSizeCm` = 100 cm on each side, so dividing the size by it gives the scale 3.0 × 1.0 × 1.3 |
| `Mesh->SetRelativeLocation(FVector(0.f, 0.f, HeightCm * 0.5f));` | The cube is centred on its origin; lifting it by half its height puts its base on the floor |
| `Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);` | `BlockAll` (`Engine/Classes/Engine/CollisionProfile.h:190`; `Config/BaseEngine.ini:3104`): blocks the Visibility channel the cover test traces, and pawns, so agents walk around it. Colliding static geometry is exported into the navmesh, which therefore has holes where blocks stand |
| `SmartObject = CreateDefaultSubobject<USmartObjectComponent>(…); SmartObject->SetupAttachment(Root);` | The Smart Object component is a scene component. At the unscaled root, its transform is the block's |
| `SetCoverDefinition(…) { SmartObject->SetDefinition(Definition); }` | Passes the shared runtime definition to the component (`SO/Public/SmartObjectComponent.h:74`) |

### B4. `Public\SQArenaSubsystem.h`

**`struct FSQCoverSlot`**: one slot in a flat table.

| Line | What it does, and why |
|---|---|
| `struct FSQCoverSlot` (no `USTRUCT`) | Plain C++. It holds no UObject pointer (`FSmartObjectSlotHandle` is a plain value), so the garbage collector doesn't need to see it, and nothing uses it in Blueprint or serialisation. Reflection would add generated code for nothing. *Alternative:* `USTRUCT()`, needed only to put it in a `UPROPERTY` |
| `FVector Location = FVector::ZeroVector;` | The slot's world position on the floor. Default member initialisers mean a new slot is never uninitialised memory |
| `int32 BlockIndex = INDEX_NONE; int32 SlotIndex = INDEX_NONE;` | The owning block and the slot's index in the definition (0..5). `INDEX_NONE` (-1) is Unreal's "unset" |
| `FSmartObjectSlotHandle Handle;` | The Smart Object runtime's id for the slot, filled when the arena becomes ready. Claims (S3) take it |

**`class USQArenaSubsystem : public UWorldSubsystem`**

| Line | What it does, and why |
|---|---|
| `#include "Containers/BitArray.h"`, `"SmartObjectTypes.h"`, `"Subsystems/WorldSubsystem.h"` | `TBitArray` for the visibility table, `FSmartObjectSlotHandle` for the slot table, and the base class |
| `UCLASS() class USQArenaSubsystem : public UWorldSubsystem` | One per world, created and destroyed with it. There is no authored map, so no placed actor would build the arena; a subsystem has exactly the right lifetime (the same reasoning as UE5-UpdateBudget's director). Not tickable: it waits for one engine event (the navmesh build finished) and one timeout timer, so it needs no per-frame tick (`06` §9, §13) |
| `virtual void OnWorldBeginPlay(UWorld& InWorld) override;` | Called once when the world starts play, **before** any actor's `BeginPlay` (UE5-UpdateBudget `01b` #7). Blocks spawned here begin play with the rest of the level, after the Smart Object subsystem has initialised its runtime in its own `OnWorldBeginPlay` (`SO/Private/SmartObjectSubsystem.cpp:3235-3240`); a component that registers earlier is queued until then (`:842-848`) |
| `virtual void Deinitialize() override;` | Called at world teardown; clears the timeout timer and unbinds from the navigation system's event |
| `bool IsReady() const` | True once navigation is built and the tables are filled. The benchmark director (S2) waits for it |
| `FSimpleMulticastDelegate OnArenaReady;` | A native multicast delegate without parameters: any number of listeners can bind. Native rather than dynamic, because no Blueprint listens and native delegates are cheaper (`06` §10) |
| `GetSlots()`, `GetThreatPositions()` | Read-only access to the shared tables (`const&`: no copy) |
| `bool IsSlotHidden(int32 ThreatIndex, int32 SlotIndex) const` | One lookup in the precomputed table: **the metrics never trace during a measured run** (`02` §4.2). Inline in the header because it runs for every agent every frame. It returns false before the arena is ready or for an index out of range, instead of asserting (*added in review*, `09` S1 audit): three cheap comparisons, so a caller that asks too early gets "not hidden", not a crash |
| `bool IsValidCover(int32 ThreatIndex, int32 SlotIndex) const` | *Added in S2's review:* hidden (`IsSlotHidden`) and at least `MinThreatDistanceCm` from the threat position. The one definition of "valid cover" for the self-check and every metric |
| `GetSeed()`, `GetArenaHash()` | The seed used, and a CRC of the layout, so two runs can prove they built the same arena. The director reads the seed from here |
| `static constexpr` layout constants | The numbers of `01` §6.1 in one place: 10 × 10 blocks 12 m apart with ±3 m jitter, a 130 m floor, slots 60 cm from the face and ±80 cm along the long sides, 10 threat positions, eye height 170 cm, probe height 90 cm (`02` §7) |
| `DoesSupportWorldType(...) const override` (protected) | Game and PIE worlds only. World subsystems also exist in editor worlds (UE5-UpdateBudget `01b` #2), where spawning an arena into the level being edited would be a bug |
| The private helpers | One function per job, in the order they run; each is explained with the `.cpp`. `HandleNavigationGenerationFinished(ANavigationData*)` is a `UFUNCTION()` because it is bound to a **dynamic** delegate, which finds its target by name through reflection (`06` §10). `class ANavigationData;` is a forward declaration: the header only names the type |
| `UPROPERTY(Transient) TObjectPtr<USmartObjectDefinition> CoverDefinition;` | The one runtime definition all blocks share. Each block's Smart Object component also references it through its own `UPROPERTY` (`SO/Public/SmartObjectComponent.h:233-234`), so the blocks alone would keep it alive; the arena holds it as well so that its owner is explicit and it survives even before the first block exists. `Transient`: never saved. *Corrected in review:* an earlier comment claimed this was the only reference |
| `UPROPERTY(Transient) TArray<TObjectPtr<ASQCoverBlock>> Blocks;` | The spawned blocks, visible to GC. A block's index is `FSQCoverSlot::BlockIndex` |
| `TArray<FSQCoverSlot> Slots; TArray<FVector> ThreatPositions;` | The shared tables. Not `UPROPERTY`: no UObject inside |
| `TBitArray<> HiddenTable;` | 10 threat positions × 600 slots = 6,000 booleans, one bit each (750 bytes). Row-major, so one threat's row is contiguous |
| `FTimerHandle TimeoutTimer; double BuildStartRealSeconds; int32 Seed = 1; uint32 ArenaHash = 0; bool bSelfCheck = false; bool bReady = false;` | Timeout state and results. `double` for `FPlatformTime::Seconds()`, which counts from process start and needs the precision (`04` §6) |

### B5. `Private\SQArenaSubsystem.cpp`

**Includes and the `SQArena` namespace**

| Line | What it does, and why |
|---|---|
| The includes | Brush component and body setup (nav bounds), static mesh actor (floor), navigation (`NavigationSystem.h`, `NavMesh/NavMeshBoundsVolume.h`, `NavigationData.h` and `NavigationSystemTypes.h` for the path test), the Smart Object component, definition, request types and subsystem, our block, the timer manager, and core helpers (command line, parsing, CRC, platform time and misc). `SmartObjectRequestTypes.h` was *added after the first S1 build failed* (`09` S1-a): the self-check builds an `FSmartObjectRequest`, whose type the subsystem header only forward-declares (`SO/Public/SmartObjectSubsystem.h:25, 27`; defined in `SO/Public/SmartObjectRequestTypes.h:86, 131`) |
| `DEFINE_LOG_CATEGORY_STATIC(LogSQArena, Log, All);` | A log category used only in this file. `Log`: messages at Log and above are printed by default; `All`: the most verbose level it can be raised to at run time |
| `namespace SQArena { … }` | File-local constants and helpers in a named namespace, so they can't collide with engine names (`04` §7) |
| `constexpr float LongSideY = 0.5f * ASQCoverBlock::ThicknessCm + USQArenaSubsystem::SlotStandoffCm;` (110) and `ShortSideX` (210) | Slot positions derived from the block size at compile time. If the block changes size, the slots follow |
| `const FVector SlotOffsets[] = { … };` | The six slots in the block's frame: (±80, ±110) on the long sides, (±210, 0) on the short sides. Adjacent slots are at least 1.6 m apart, so two different slots are never close enough to look like one (`01` §6.3). `const`, not `constexpr`, because `FVector`'s constructor isn't `constexpr` |
| `constexpr int32 SlotsPerBlock = UE_ARRAY_COUNT(SlotOffsets);` | 6, counted by the compiler, so the count and the array can't disagree |
| `NumBlocks` (100), `NumSlots` (600), `GridHalfExtentCm` (5400) | Derived once from the layout constants and used by the grid, the threat placement, the reservations and the self-check, so each fact lives in one place (*review*) |
| `NavTimeoutRealSeconds = 60.0` | **Real** time: a fixed-step run can simulate many world seconds per real second, while the navmesh builds on worker threads in real time |
| `CornerCm = GridHalfExtentCm + GridJitterCm + ShortSideX + 290.f` (6200) and its `static_assert` | The path-check corners lie beyond every block and slot (block centre + jitter + the farthest slot, plus a 2.9 m margin), and the `static_assert` proves at compile time that they stay on the 65 m half-floor. *Corrected before the first run:* a first draft used 58 m, where a jittered edge block could cover a corner and fail the check for the wrong reason; the value is now derived, so a layout change can't reintroduce that |
| `NavBoundsHeightCm = 1000.f` | The bounds box spans 5 m below and above the floor |
| `ThreatClearanceCm = 260.f` | A threat never stands inside or against a block (half-length 150 cm plus a margin) |
| `MinHiddenSlotsPerThreat = 256` | The S1 pass threshold: at least 2 × 128 usable hidden slots from every threat position (`03`, S1). The 8 m distance itself moved to the class as `USQArenaSubsystem::MinThreatDistanceCm` in S2, next to the eye and probe heights, so the arena, the director and the queries share one constant |
| `bool IsPathComplete(const UNavigationSystemV1&, From, To)` | One synchronous path **test**: `TestPathSync(FPathFindingQuery(nullptr, *NavData, From, To))` (`NS/Public/NavigationSystem.h:679`). It builds no `UNavigationPath` object for the garbage collector, and a partial path counts as a failure (`NS/Private/NavMesh/PImplRecastNavMesh.cpp:310-314`; `RecastNavMesh.cpp:3920-3921`). The query takes the navmesh's default filter when none is given (`NS/Private/NavigationData.cpp:53-56`). *Changed in review:* the first version called `FindPathToLocationSynchronously` and read `IsValid() && !IsPartial()` from the path object |
| `bool SpansArena(const UNavigationSystemV1&)` | The readiness test: `IsPathComplete` along the diagonal, from corner to opposite corner. Used by the build-finished handler and by the timeout's diagnostic line |
| `void RequestSelfCheckExit(bool bPass)` | The self-check's verdict as the exit code, in one place: a normal exit on PASS (code 0); a **forced** exit with code 1 on FAIL, because only the forced path terminates the process with that code, while the normal path posts a quit message (`Core/Private/Windows/WindowsPlatformMisc.cpp:1493-1520`; the exception cvar it checks is off by default, `:140-144`). Used by the timeout and by `FinishArena` |

**`DoesSupportWorldType`** returns true for `EWorldType::Game` and `EWorldType::PIE` only.

**`OnWorldBeginPlay`**

| Line | What it does, and why |
|---|---|
| `Super::OnWorldBeginPlay(InWorld);` | The base class first |
| `FParse::Value(CommandLine, TEXT("SQSeed="), Seed);` | Reads `-SQSeed=N` if present; otherwise `Seed` stays 1 |
| `bSelfCheck = FParse::Param(CommandLine, TEXT("SQArenaCheck"));` | A flag without a value |
| `NavSys->OnNavigationGenerationFinishedDelegate.AddUniqueDynamic(this, &USQArenaSubsystem::HandleNavigationGenerationFinished);` | Listen for the navigation system's "a navmesh build finished" event **before** anything exists that could start a build, so no build can finish unheard. It is broadcast whenever the Recast generator's task count drops to zero (`NS/Private/NavMesh/RecastNavMeshGenerator.cpp:7614-7631` → `RecastNavMesh.cpp:3237-3240` → `NavigationSystem.cpp:4913-4915`), outside `WITH_EDITOR`, so in packaged builds too. `AddUniqueDynamic` binds a `UFUNCTION` by name and never twice (`06` §10). No navigation system → an Error line |
| `BuildStartRealSeconds = FPlatformTime::Seconds(); BuildArena(InWorld);` | When the build started (timeout and logs), then spawn everything |
| `SetTimer(TimeoutTimer, this, &USQArenaSubsystem::CheckNavigationTimeout, 60.f, /*bLoop*/ false);` | One timeout, not a poll. A timer bound to a UObject is skipped if the object is gone (`06` §9) |

*Changed in review, then corrected by the first self-check run* (`09` S1 audit, S1-c): the first version polled every 0.1 s and called the arena ready when a navmesh existed, nothing was building, and a corner-to-corner path was complete. The review replaced the polling with the engine's build-finished event and dropped the path test. The next run showed why the path test must stay: the event **also fires for a build that covers almost nothing** (that run's bounds were empty, see `SpawnNavigationBounds`), and the arena reported ready on frame 1 with no usable navmesh. Now readiness is the event **plus** one path test per event, which costs nothing between events, and the timeout reports what it saw.

**`Deinitialize`** clears the timeout timer and unbinds from the navigation system if the world still exists, then calls `Super::Deinitialize()` last: undo our own state before the base class tears down its own.

**`BuildArena`**

| Line | What it does, and why |
|---|---|
| `FRandomStream Stream(Seed);` | A seeded random sequence: the same seed gives the same numbers in every run and build. `FMath::Rand` would not (UE5-UpdateBudget `07`) |
| `UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, ASQCoverBlock::CubeMeshPath);` | A synchronous load, once, before anything is measured. The floor isn't our own class, so there is no constructor to find the mesh in. A missing cube is logged as an Error (*added in review*): without it nothing has collision and the only other symptom would be the 60 s navigation timeout |
| `const FTransform FloorTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -0.5f * FloorThicknessCm), FVector(FloorSizeCm, FloorSizeCm, FloorThicknessCm) / ASQCoverBlock::CubeSizeCm);` | The cube scaled to 130 m × 130 m × 20 cm, centred 10 cm below zero, so the **top face is at z = 0**. Scaling this actor is fine: it has no slot offsets |
| `SpawnActor<AStaticMeshActor>` → `SetStaticMesh` → `SetCollisionProfileName(BlockAll)` | A plain spawn. Its components register during the spawn itself (`Engine/Private/Actor.cpp:4330-4336`), so the mesh is set on an already registered component; `SetStaticMesh` then updates collision and the navigation data itself (`StaticMeshComponent.cpp:2418-2429`). Swapping the mesh of a registered **Static** component is refused once the world has begun play (`StaticMeshComponent.cpp:2374-2383`), and `OnWorldBeginPlay` runs before that: the world calls it on every subsystem before the game mode's `StartPlay` (`World.cpp:6165-6174`), which leads to `AWorldSettings::NotifyBeginPlay` setting begun play (`WorldSettings.cpp:377`). `BlockAll` makes the floor walkable and part of the navmesh. *Corrected in review:* the first version used a deferred spawn and claimed it registered the components only in `FinishSpawning`; for an actor with a native root that is false, so the deferral did nothing |
| `UE_LOG(LogSQArena, Error, …)` on a failed spawn | Spawning can fail (for example while the world shuts down). An Error line fails the run's log check instead of silently missing the floor |
| `CoverDefinition = CreateCoverDefinition();` | One definition for all 100 blocks |
| `const float GridOrigin = -SQArena::GridHalfExtentCm;` | -54 m: centres the 10 × 10 grid on the origin (columns at -54, -42, …, +54 m) |
| `Blocks.Reserve(…); Slots.Reserve(…);` | Allocate once instead of growing 100 and 600 times |
| `const float JitterX = Stream.FRandRange(…);` `const float JitterY = …;` `const float Yaw = Stream.RandRange(0, 1) * 90.f;` | **One random draw per statement.** C++ doesn't specify the order in which function arguments are evaluated, so `FVector(Stream.Draw(), Stream.Draw(), 0)` could swap the two draws between compilers and change the arena (`04` §8). Yaw is 0° or 90° |
| `const FTransform BlockTransform(FRotator(0.f, Yaw, 0.f), Location);` | The block's place and heading |
| `SpawnActorDeferred<ASQCoverBlock>` → `SetCoverDefinition` → `FinishSpawning` | The definition is set before `BeginPlay` registers the Smart Object component (B2). Deferral matters here and not for the floor: the Smart Object component registers with its subsystem in `BeginPlay`, which `FinishSpawning` triggers when the world is already playing; at `OnWorldBeginPlay` it would run later anyway, but the block stays correct if the arena is ever built after begin play. On failure: an Error line and `continue`, so the tables never point at a block that doesn't exist |
| `const int32 BlockIndex = Blocks.Add(Block);` | `Add` returns the new element's index, which becomes the block's id in the slot table |
| `Slot.Location = BlockTransform.TransformPosition(SQArena::SlotOffsets[SlotIndex]);` | Our own copy of where the Smart Object runtime puts each slot (block transform × slot offset). The self-check compares the two |
| `Crc = FCrc::TypeCrc32(Location, Crc); Crc = FCrc::TypeCrc32(Yaw, Crc);` | Everything that defines a block goes into the hash. `TypeCrc32` adds a value's bytes to a running CRC-32 (`Core/Public/Misc/Crc.h:36-39`). *Changed in review:* a hand-written `HashVector` helper did the same for vectors only; the hash is bit-identical. The hash covers the exact bits, so it is compared between runs of **one binary**: the editor binary and the packaged game round some float expressions differently (`/fp:precise` vs `/fp:fast`, `01b` #21), and their hashes differ for the same seed while the layout is the same (`09` S1) |
| `PlaceThreatPositions(Stream);` then the threat positions into the hash; `ArenaHash = Crc;` | The threat positions continue the **same** random stream, so they also depend only on the seed |
| `SpawnNavigationBounds(World);` | Last, so the navmesh is built once with every block already in place |
| The `Display` log line | Seed, counts, hash and build time (from `BuildStartRealSeconds`, taken just before the call): what a reviewer needs to compare runs |

**`CreateCoverDefinition`**

| Line | What it does, and why |
|---|---|
| `NewObject<USmartObjectDefinition>(this, TEXT("SQCoverDefinition"), RF_Transient)` | A data asset created as a plain object at runtime, owned by the subsystem, never saved. The way Epic's Smart Object tests build one (`SmartObjectsTestSuite/Private/SmartObjectsTest.cpp:65-72`) |
| `FSmartObjectSlotDefinition& SlotDefinition = Definition->DebugAddSlot();` | Adds a default slot and returns a reference to it (`SO/Public/SmartObjectDefinition.h:326-327`). Named "Debug" and documented "for testing purposes", but not editor-only, and the only public way to add a slot from code. A shipping game would author the definition as an asset |
| `SlotDefinition.Offset = FVector3f(Offset);` | The slot's position relative to the block (`SmartObjectDefinition.h:177-179`). `FVector3f` is the single-precision vector the definition stores |
| `SlotDefinition.BehaviorDefinitions.Add(NewObject<USQCoverBehaviorDefinition>(Definition));` | One behavior definition per slot, owned by the definition, so validation passes (`01b` #19) |

**`SpawnNavigationBounds`**

| Line | What it does, and why |
|---|---|
| `SpawnActor<ANavMeshBoundsVolume>(…)` | The volume that tells navigation where to build. Its components register during the spawn, which already announces the volume, with an empty box (next rows). *Corrected in review:* the first version spawned it deferred "so its bounds exist before registration"; deferral doesn't delay registration for an actor with a native root (`Actor.cpp:4330-4336`) |
| `UBodySetup* Body = NewObject<UBodySetup>(Brush, NAME_None, RF_Transient);` | A volume spawned at runtime has no editor brush. Outside the editor its brush component takes its bounds from `BrushBodySetup->AggGeom` (`Engine/Private/Components/BrushComponent.cpp:476-481`), so we give it a body setup |
| `Body->AggGeom.BoxElems.Emplace(FloorSizeCm, FloorSizeCm, SQArena::NavBoundsHeightCm);` | One box of 130 m × 130 m × 10 m, centred on the actor. `FKBoxElem(X, Y, Z)` takes **full lengths**; the bounds use half of each (`Engine/Private/Collision/KAggregateGeom.cpp:391`). `Emplace` constructs the element in place |
| `Brush->BrushBodySetup = Body;` | A public `UPROPERTY` (`Engine/Classes/Components/BrushComponent.h:28-30`). The brush keeps its `NoCollision` profile, so the box creates no physics body; it only defines bounds |
| `Brush->UpdateBounds();` | **Added after S1-c.** A component's bounds are **cached**: computed when it registers and when it moves, not when its body setup changes. The brush registered during the spawn, with no body, so its cached bounds were a point at the origin; the navigation system reads that cached value (`Engine/Private/Actor.cpp:2272-2274`, from `NS/Private/NavigationSystem.cpp:4145`) and skipped it as "empty bounds" (`LogNavigationDirtyArea: Warning: … 1 empty bounds`). `UpdateBounds()` (`Engine/Classes/Components/SceneComponent.h:1130`) recomputes them from the box (`BrushComponent.cpp:476-481`) |
| `NavSys->OnNavigationBoundsUpdated(Volume);` | **The call that gives the navmesh its real bounds.** Registration during the spawn already announced the volume (`PostRegisterAllComponents`, `NS/Private/NavMesh/NavMeshBoundsVolume.cpp:117-127`) while its box was still empty. This second announcement, with the box body in place, replaces that pending request instead of adding one (`NS/Private/NavigationSystem.cpp:4261-4298`), so the navmesh is built once, for the whole arena. The code comment says not to remove it |

**`PlaceThreatPositions`** draws random points within ±54 m and keeps those at least 2.6 m (horizontally) from every block centre, until 10 are found or 10,000 attempts are spent. `ContainsByPredicate` takes a lambda that captures the candidate by reference (`04` §9). Fewer than 10 → an Error line.

**`HandleNavigationGenerationFinished`** (the engine event)

| Line | What it does, and why |
|---|---|
| `if (bReady \|\| World == nullptr \|\| NavSys == nullptr \|\| NavSys->IsNavigationBuildInProgress() \|\| !SQArena::SpansArena(*NavSys)) return;` | Finish once. A build has just finished; if more work is already queued (`IsBuildInProgressCheckDirty` counts the generator's running and pending tiles, `RecastNavMeshGenerator.cpp:7764-7769`), wait: that work's own completion fires the event again. And the navmesh must **span the arena**: the event also fires for a build that covers almost nothing (S1-c), so one path test per event decides. If it fails, the next build's event or the timeout decides |
| `ClearTimer(TimeoutTimer); FinishArena(*World);` | Cancel the timeout; finish |

**`CheckNavigationTimeout`**

| Line | What it does, and why |
|---|---|
| `RealSecondsLeft = 60 - (FPlatformTime::Seconds() - BuildStartRealSeconds)`; if > 0, `SetTimer(…, RealSecondsLeft, false)` and return | Timers count **world** time; the navmesh builds on worker threads in **real** time. In a fixed-step run with fast frames, world time runs ahead, so the timer re-arms itself until 60 real seconds have passed |
| Otherwise: an Error line saying whether a navmesh exists and whether the corner-to-corner path is complete, and in check mode `RequestSelfCheckExit(false)` | No usable navmesh after a minute of real time: the run is broken, the log says which half failed, and the queue job fails with exit code 1 |

**`FinishArena`** logs how long navigation took (real time, from the event), fills the slot handles, traces the visibility table, sets `bReady`, broadcasts `OnArenaReady`, and in check mode passes the self-check's verdict to `RequestSelfCheckExit`: a normal quit on PASS (exit code 0), forced with code 1 on FAIL, so the queue job fails visibly.

**`FillSlotHandles`**

| Line | What it does, and why |
|---|---|
| `USmartObjectSubsystem::GetCurrent(&World)` | The world's Smart Object subsystem (`SO/Public/SmartObjectSubsystem.h:301`) |
| `if (Slot.SlotIndex == 0) { … if (Block && Block->GetSmartObjectComponent()) GetAllSlots(…GetRegisteredHandle(), BlockSlots); }` | Once per block: the handles of all its slots (`SmartObjectSubsystem.h:536`), in definition order, the same order the table was filled in. A block destroyed before this point is skipped instead of dereferenced (*added in review*) |
| `if (BlockSlots.IsValidIndex(Slot.SlotIndex)) { Slot.Handle = …; ++WithHandle; }` | A block that failed to register leaves its slots without handles |
| `UE_CLOG(WithHandle != Slots.Num(), …, Error, …)` | *Added in review:* a definition that fails validation makes its components skip registration with only a `Log`-level line (`01b` #19), so every run, not just the self-check, now reports missing handles as an Error |

**`BuildVisibilityTable`**

| Line | What it does, and why |
|---|---|
| `const FCollisionQueryParams Params(SCENE_QUERY_STAT(SQArenaVisibility), false);` | Names the traces for profiling and traces simple collision (the blocks are boxes) |
| `HiddenTable.Init(false, ThreatPositions.Num() * Slots.Num());` | Allocates every bit, all false |
| `HiddenTable[ThreatIndex * Slots.Num() + SlotIndex] = World.LineTraceTestByChannel(Eye, Chest, ECC_Visibility, Params);` | "Is anything between the threat's eye and the slot at chest height?" A hit means hidden. Same channel and heights as the cover query's trace test (`02` §7), so the metric and the AI agree on "hidden". Pawns ignore Visibility (`Config/BaseEngine.ini:3110`), so agents never block it. 6,000 traces, once, before any agent exists |

**`RunSelfCheck`** is the S1 check of `03`; every result goes on one `SQArenaCheck PASS/FAIL` log line.

| Check | Code | Pass when |
|---|---|---|
| The Smart Object runtime knows every slot | `FindSmartObjects(FSmartObjectRequest(ArenaBox, FSmartObjectRequestFilter()), Found, FConstStructView())`: an unfiltered spatial query over the whole arena, without user data | `Found.Num()` = 600 |
| Every slot has a handle, the runtime agrees on its position, and it lies on the navmesh | `GetSlotTransformChecked(Handle).GetLocation()` vs our location; `ProjectPointToNavigation(Location, Projected, (50, 50, 150))` | 600 handles, error < 1 cm, 600 on the navmesh |
| Paths cross the arena | `IsPathComplete` on both diagonals and two edges, from one table of corner pairs | 4 of 4 |
| Enough cover for 128 agents from every threat position | Per threat: hidden slots, and hidden slots at least 8 m from the threat | the smallest "hidden and ≥ 8 m" count ≥ 256 |
| Layout | `Slots.Num()`, `ThreatPositions.Num()` | 600 and 10 |

---

## Part C · S2: agents, the benchmark director and the measurement scripts

S2 adds everything a run needs **except a real decision method**: agents that walk, a director that runs the threat timeline and measures decision quality, and the scripts that launch, pin and summarise runs. Decisions come from a scripted test oracle, so the metrics can be checked against a case whose answer is known (`03`, S2).

### C1. `SquadQuery.Build.cs` (changed in S2)

| Line | What changed, and why |
|---|---|
| `PublicDependencyModuleNames … "AIModule"` | `SQAgent.cpp` uses `AAIController`, the brain calls `MoveToLocation` and listens to path following, and `SQAgentBrainComponent.h` stores an `FAIRequestID` (`AIModule/Classes/AITypes.h`). A public header that needs a module's types makes that module a **public** dependency (`06` §2) |
| `NavigationSystem` comment | The director now projects spawn points onto the navmesh too; still only in `.cpp` files, so still private |

### C1b. `Config\DefaultEngine.ini` (changed in S2)

| Line | What changed, and why |
|---|---|
| `[/Script/NavigationSystem.NavigationSystemV1] CrowdManagerClass=` | No Detour crowd manager. The navigation system creates one from this class (default set in `NS/Private/NavigationSystem.cpp:931`), and an empty class creates none (`:1570-1579`). Our agents use the AI controller's ordinary path following, not crowd following, so nothing needs it; with it, every run that built a navmesh logged `LogCrowdFollowing: Warning: Unable to find RecastNavMesh instance while trying to create UCrowdManager instance` (`AIModule/Private/Navigation/CrowdManager.cpp:905`) at world teardown (`09` S1, observation 2). UE5-NavReadyPCG made the same change for the same warning |
| `[/Script/EngineSettings.GameMapsSettings] GlobalDefaultGameMode=/Script/SquadQuery.SQGameMode` | Our game mode (C2b) instead of `AGameModeBase`. The template map sets no game mode of its own, so the global one applies; the log line `LogLoad: Game class is 'SQGameMode'` confirms it (`09` S2) |

### C2. `Public\SQAgent.h` / `Private\SQAgent.cpp`

| Line | What it does, and why |
|---|---|
| `UCLASS() class ASQAgent : public ACharacter` | The agent of every configuration. A character, because character movement plus an AI controller following navmesh paths is the standard Unreal setup the baselines would use (`02` §4.2). The same class in every configuration, so movement cost is identical and only decisions differ |
| `GetBrain() const` | The director and the metrics read the brain |
| `static constexpr float WalkSpeedCmPerSecond = 450.f;` | A jog. Time-to-cover depends on it, so it is one constant for every configuration (`01` §6.1) |
| `UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;` and `Brain` | The visible cylinder and the decision component, both GC-visible |
| `PrimaryActorTick.bCanEverTick = false;` | The actor's own tick does nothing; movement ticks in its component, decisions in the brain. One fewer tick function per agent |
| `AIControllerClass = AAIController::StaticClass(); AutoPossessAI = EAutoPossessAI::Spawned;` | Every spawned agent gets a plain engine AI controller. Path following comes from it; decisions don't, so the controller is identical in every configuration |
| `Movement->MaxWalkSpeed = WalkSpeedCmPerSecond; Movement->bOrientRotationToMovement = true; bUseControllerRotationYaw = false;` | Walk at the fixed speed and face where you walk, the usual AI character setup |
| `Movement->bUseRVOAvoidance = true;` | **Added after the first S2 run** (`09` S2-a). Path following plans around blocks, not around other agents. Without avoidance, an agent whose path ran through a squadmate already standing in a neighbouring slot pushed into it until path following declared the move blocked; 19 of 320 agent-events (6%) ended short of cover, every one with its capsule touching another agent's. RVO avoidance (`Engine/Classes/GameFramework/CharacterMovementComponent.h:1065`) steers each agent's velocity around nearby agents. It is the engine's standard answer for characters, and it costs the same in every configuration. With it (S2-b): no agent stuck at the end, 98.8% of agent-events in cover before the next event, and a walk ratio of 1.14 instead of 1.00 (steering around others adds detours) |
| `static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(…)` | The engine cylinder, found once per class (`05` B3) |
| `Body->SetRelativeScale3D(FVector(2 * Radius, 2 * Radius, 2 * HalfHeight) / 100.f);` | Sizes the 100 cm cylinder to the default capsule (radius 34, half-height 88), read from the capsule so the two can't disagree |
| `Body->SetCollisionProfileName(NoCollision); Body->SetCanEverAffectNavigation(false);` | Only the capsule collides; the visual body is neither physics nor navigation geometry |
| (inherited) the capsule | The engine sets it up so agents change neither the visibility answer nor the navmesh: the `Pawn` profile ignores the `Visibility` channel (`BaseEngine.ini:3110`), and a character's capsule never affects navigation (`Character.cpp:83`). So the visibility table the arena traces **before** agents exist (`05` B5) stays the answer the EQS trace test gives **while** they move, and 128 moving capsules don't trigger navmesh rebuilds |

### C2b. `Public\SQGameMode.h` / `Private\SQGameMode.cpp`

| Line | What it does, and why |
|---|---|
| `UCLASS() class ASQGameMode : public AGameModeBase` | The engine's default game mode with one change. *Added in review:* the first version let `AGameModeBase` spawn its `ADefaultPawn` at the template map's `PlayerStart` and had the director destroy it, and re-applied the camera every frame in case something possessed later. Removing the pawn at its source makes both workarounds unnecessary |
| `DefaultPawnClass = nullptr;` (constructor) | `DefaultPawnClass` is a plain class property, not a config one (`Engine/Classes/GameFramework/GameModeBase.h:106-108`), hence a subclass. With no class the engine skips the spawn (`Engine/Private/GameModeBase.cpp:1292-1305`) and the controller just goes inactive (`Controller.cpp:115-118`; `PlayerController.cpp:354-359`), without logging anything |

### C3. `Public\SQAgentBrainComponent.h` / `Private\SQAgentBrainComponent.cpp` (S2 version)

| Line | What it does, and why |
|---|---|
| `struct FSQBrainCounters` (switches, oscillations, move requests, retries, failures) with `operator-` | Everything one brain counts, as plain numbers. The director copies it at `BenchStart` and subtracts the copy at the end: one snapshot and one subtraction, however many counters later steps add (*review:* the first version kept a separate `…AtStart` field per counter) |
| `void Setup(int32 InSquadIndex)` | The squad, for the run report, set once before the agent finishes spawning |
| `void SetTarget(int32 SlotIndex)` | **The one place a decision becomes movement** (`02` §9): every configuration ends here |
| `static constexpr double OscillationWindowSeconds = 3.0;` | Taking again, within 3 s, the slot held before the current one counts as an oscillation (`01` §6.3) |
| `static constexpr float AcceptanceRadiusCm = 50.f;` | How close `MoveToLocation` must get. The "in cover" test allows 75 cm (`02` §7) |
| `UPROPERTY(Transient) TObjectPtr<USQArenaSubsystem> Arena;` and `Controller` | Cached: the slot table is needed on every decision, and the controller on every move (`ResolveController`) |
| `FAIRequestID CurrentMoveId;` | The path-following request this brain is waiting for, so a finished-move notification can be matched to it |
| `TargetSlot`, `PreviousSlot`, `PreviousSlotLeftTime` | The current decision, and the slot held before it, for oscillation detection |
| `PrimaryComponentTick.bCanEverTick = false;` (S2) | In S2 decisions arrive from outside. The polled baselines (S3) turn ticking on |
| `ResolveController()` | The pawn's `AAIController`, cast once and cached (`AutoPossessAI = Spawned` possesses during the spawn, before `BeginPlay`; resolved lazily as well, in case a decision arrives first). On first use it subscribes to the path-following component's `OnRequestFinished` (`AIModule/Classes/Navigation/PathFollowingComponent.h:229`) |
| `CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SquadQueryDecide);` | The project's AI-time stat (`01` §6.3). **Exclusive**: time spent inside a nested exclusive scope is subtracted. So when this code runs inside the EQS tick (S3) it is counted here and not in `EnvQueryManager` (`01b` #5); and the engine's path query inside `MoveToLocation` opens its own exclusive `Pathfinding` scope (`NS/Private/NavMesh/RecastNavMesh.cpp:3839`), which is why AI time adds that column back (C5) |
| `if (SlotIndex == TargetSlot) return;` | The same decision again costs nothing and isn't a switch |
| `if (SlotIndex != INDEX_NONE && SlotIndex == PreviousSlot && Now - PreviousSlotLeftTime <= OscillationWindowSeconds) ++Oscillations;` | Checked on every change, before the bookkeeping. Stops are transparent: A → none → A and A → none → B → A both count (*review:* the first version skipped the check after a stop) |
| `if (TargetSlot != INDEX_NONE) { ++Switches; PreviousSlot = TargetSlot; … }` | A switch is leaving a slot. Going from no slot to a slot (the first decision, or after a stop) is not a switch |
| `StopMovement()` for `INDEX_NONE` | No slot: stand still, and forget the request id |
| `RequestMove`: `MoveToLocation(Slot.Location, 50, false, true, true, false)` | Walk to the slot: acceptance 50 cm, don't stop on overlap, use pathfinding, project the destination onto the navmesh, don't strafe. **Every call runs a synchronous path query** (`01b` #15), which is why switches cost CPU. It counts the request, counts a `Failed` result, and keeps the request id when the request was accepted (`AAIController::GetCurrentMoveRequestID`) |
| `OnMoveFinished(RequestID, Result)` | Path following reports a finished request. Only our current request, and only when it ended **short of the slot** (blocked by another capsule, off its path, invalid path) matters; success and interruptions (`IsInterrupted()`: a newer request or `StopMovement`) don't. The retry waits for the next frame, outside path following's own callback |
| `RetryMove()` | Issues the move again, inside `SquadQueryDecide`, counted as a retry. *Changed in review:* a first fix re-issued the move only when a chooser happened to pick the same slot again, so the retry rate depended on the configuration (every 0.5 s for the polled baselines, never for the event-driven ones). Driving it from the engine's finished-move event makes it identical in every configuration. Path following detects "blocked" itself (less than 10 cm progress over its sampling window, `AIModule/Private/Navigation/PathFollowingComponent.cpp:138-144`), so retries come seconds apart, not every frame |

### C4. `Public\SQBenchmarkSubsystem.h` / `Private\SQBenchmarkSubsystem.cpp`

**Header**

| Line | What it does, and why |
|---|---|
| `UCLASS() class USQBenchmarkSubsystem : public UTickableWorldSubsystem` | The director. A tickable world subsystem: the timeline and the metrics run every frame (UE5-UpdateBudget's pattern) |
| `GetCurrentThreatIndex()` | The current threat position (index into the arena's list) |
| `InCoverRadiusCm = 75.f`, `SquadSize = 8`, `EventOffsetSeconds = 0.25` | "In cover" radius, squad size (`01` §6.1), and the offset that keeps the first captured event off the `BenchStart` frame |
| `struct FRunSettings` | Every `-SQ…=` option with its default |
| `enum class ERunPhase { Idle, WaitingForArena, WarmUp, Capture, Done }` | The run's states, in order |
| `struct FAgentMetrics` | Per agent: the event it is still getting into cover for, where it stood when that event fired, and its brain's `FSQBrainCounters` at `BenchStart`. The event's own `Time` is the start of the wait, so no separate timestamp is kept (*review*) |
| `struct FArrival { float Seconds = -1.f; float DistanceCm = -1.f; }` | How one agent answered one event. One array of these per event instead of two parallel arrays (*review*) |
| `struct FEventRecord` | One threat event: which position, when, whether it was captured, and every agent's `FArrival` (both `-1` = not in cover before the window closed). Distance ÷ walking speed is the fastest possible time, so the two together separate "decided late or walked around blocks" from "had far to go" |
| `UPROPERTY(Transient)` on `Arena`, `Agents`, `Camera`, `ThreatMarker` | GC-visible references to everything the director spawned or uses |
| `TArray<uint8> SlotTargetCounts; TBitArray<> SlotInConflict;` | Per slot: how many agents target it this frame, and whether it was in conflict last frame (to count onsets) |
| Capture totals (`CapturedEvents`, `CapturedFrames`, `ConflictOnsets`, `ConflictAgentFrames`, `InCoverAgentFrames`) | Summed only during the capture. Agent-frames are `CapturedFrames × Agents.Num()`, computed where needed rather than counted (*review*) |

**`ReadRunSettings` / `ValidateRunSettings`**: parse `-SQMode=`, `-SQAgents=`, `-SQWarmup=`, `-SQCapture=`, `-SQEventInterval=`, `-SQRunTag=`, `-SQExit`. The seed is not parsed again: the director uses the arena's (`Arena->GetSeed()`), so there is one source for it (*review*). S2 accepts only `Scripted`; an unknown mode, a negative warm-up, a non-positive capture or an event interval under 1 s is an Error and no run starts. At most one event fires per frame, so a shorter interval would drift behind the timeline (*review*)

**`OnWorldBeginPlay` / `HandleArenaReady`**: with no agents requested, nothing happens. Otherwise wait for the arena: start at once if it is ready, else bind to `OnArenaReady` (`06` §10). Agents can only spawn on a navmesh, so the run can't start earlier.

**`StartRun`**

| Line | What it does, and why |
|---|---|
| `SpawnSquads(World); AgentMetrics.SetNum(…); SlotTargetCounts.SetNumZeroed(…); SlotInConflict.Init(false, …);` | Spawn, then size the per-agent and per-slot state once |
| `World.SpawnActor<ACameraActor>(FVector(0, 0, 10000), FRotator(-90, 0, 0))` | A top-down camera 100 m up: with the default 90° field of view it sees the whole 130 m floor. The same view in every configuration, so rendering cost is the same |
| `PlayerController->bAutoManageActiveCameraTarget = false; PlayerController->SetViewTarget(Camera);` | Set once. The player has no pawn (C2b), and with auto-management off (`Engine/Classes/GameFramework/PlayerController.h:299`) nothing resets the view target, so no per-frame re-check is needed (*review*) |
| `Events.Reserve(…)` | Room for every event of the run, allocated once |
| The threat marker (`AStaticMeshActor`, sphere, `Movable`, `NoCollision`, no navigation) | Only for the viewer. Movable because it relocates; no collision, so it never blocks a trace or a path |
| `NextEventTime = StartTime + FirstCapturedEvent - floor(FirstCapturedEvent / Interval) * Interval;` | Events are anchored on the **captured** ones (`BenchStart` + 0.25 s + k × 6 s); stepping back whole intervals gives the warm-up events. With the defaults: 4.25 s (warm-up), then 10.25 s, 16.25 s, … |
| `CSV_METADATA(TEXT("SQRunTag"), …)` and the others | The run's description inside the CSV itself, so every capture is self-describing (UE5-UpdateBudget's convention) |

**`SpawnSquads`**

| Line | What it does, and why |
|---|---|
| `FRandomStream Stream(Arena->GetSeed() + 1000);` | Its own stream: the arena depends only on the seed, and the agents don't change it |
| `GetDefault<ASQAgent>()->GetDefaultHalfHeight() + 2.f` | Spawn height from the class default's capsule (`Engine/Private/Character.cpp:263-269`), so the capsule rests on the floor. *Corrected in review:* the first version called `GetSimpleCollisionHalfHeight()` on the class default object, which returns 0 there (its capsule never registered) and logs "Called on default object … Consider using GetDefaultHalfHeight()" (`Character.cpp:234-239`): every agent would have spawned 86 cm into the floor |
| Squad centres: random within ±50 m, at least 14 m apart | Squads start apart, each in its own part of the arena |
| `ProjectPointToNavigation(Point, OnNavmesh, FVector(10, 10, 200))` for each of the 8 ring points | A ring point must lie on the navmesh within 10 cm horizontally: open floor, not inside or against a block. A centre whose ring doesn't fit is skipped |
| `SpawnActorDeferred<ASQAgent>(…, AdjustIfPossibleButAlwaysSpawn)` → `Setup(SquadIndex)` → `FinishSpawning` | The squad before `BeginPlay`; the collision handling nudges an agent that would overlap, instead of failing |
| `Crc = FCrc::TypeCrc32(Point, Crc)` | A start-position hash, logged, so two runs can prove they started identically |

**`Tick`**: at warm-up end, snapshot every brain's counters, clear the per-slot conflict state (so a conflict already running when the capture starts counts as a captured onset, *review*) and fire `CSV_EVENT(SQBench, "BenchStart")`; during warm-up and capture, fire due events and update the metrics; at capture end, fire `BenchEnd` and log a summary; in `Done`, write the report on the first frame and, with `-SQExit`, quit on the third so the CSV profiler finishes its file (UE5-UpdateBudget).

**`FireThreatEvent`**: records the event (threat position = event number modulo 10, captured or not, `-1` for every agent), stores each agent's position, moves the marker, fires `CSV_EVENT(SQBench, "Threat")` so event frames can be found in the CSV (H6), opens a cover window for every agent, and in `Scripted` mode calls `AssignScripted`.

**`IsSlotValidCover` / `IsInCover`**: a slot is valid cover if `USQArenaSubsystem::IsValidCover` says so for the current threat position: hidden in the visibility table and at least 8 m away, the same rule as the cover query's two filters (`02` §7), and the same function the S1 self-check uses (*review:* the director first had its own copy of the rule and of the 8 m constant). An agent is in cover if its target slot is valid cover and it stands within 75 cm of it. **No traces**: one bit lookup and two distances.

**`AssignScripted`** is a **test oracle, not a configuration**. It knows the visibility table, so its decisions cost no queries. Pass 1 keeps every still-valid target; pass 2 gives each other agent, in index order, the nearest free valid slot. With it, the metrics have a known answer: no conflicts by construction, and time-to-cover close to distance ÷ speed.

**`UpdateMetrics`** (inside `CSV_SCOPED_TIMING_STAT(SQBench, Metrics)`, which is not part of AI time)

| Line | What it does, and why |
|---|---|
| `FMemory::Memzero(SlotTargetCounts.GetData(), SlotTargetCounts.Num());` | Clear 600 counters (600 bytes) |
| For each agent: count its target slot; test `IsInCover`; if it is waiting for an event and is now in cover, record `Now - Event.Time` and the distance from its position at the event to that slot | Time to cover is measured from the event frame to the first frame in cover |
| For each slot: a conflict is 2+ agents; count an **onset** when a slot enters conflict during the capture | One long conflict counts once |
| `CSV_CUSTOM_STAT(SQBench, InCover/Awaiting/ConflictSlots, …, Set)` | Three per-frame counts in the capture |

**`WriteRunReport`**: two CSV files in `Saved/SQRuns/` (moved into `Benchmarks/Raw/` by the run script), each starting with one `# key=value …` line with the run description (including `walkspeed`, from `ASQAgent::WalkSpeedCmPerSecond`, so the scripts never hard-code it) and the capture totals: `<tag>_agents.csv` (per agent: switches, oscillations, move requests, retries and failures during the capture, final target and position, and the final distance to that slot: above 75 cm the agent never got there) and `<tag>_events.csv` (per captured event and agent: seconds to cover and distance, or `-1`).

### C5. `Tools\csv_stats.py` and `Tools\run_bench.py`

Copied from UE5-UpdateBudget and adapted. What changed, and why:

| Script | Change | Why |
|---|---|---|
| `csv_stats.py` | **Composite columns** `NAME=A+B`: the per-frame sum of two columns, an empty cell counting 0 | AI time is the sum of two exclusive stats per frame (`01` §6.3); summing their separate percentiles would be wrong |
| `csv_stats.py` | `AI_COLUMN = EnvQueryManager + SquadQueryDecide + Pathfinding` (all three exclusive) | *Changed in review:* the engine's path query opens its own exclusive scope inside ours (C3), so without the third part AI time would leave out the path planning every decision starts, and favour configurations that switch often. The column is global, so it would also count repaths the navigation system starts itself; with a static arena there are none |
| `csv_stats.py` | Default columns: frame and thread times, `AI`, its three parts, `SQBench/InCover`; the `mean` is printed. `run_bench.py` uses the same list (`SUMMARY_COLUMNS = DEFAULT_COLUMNS`) | AI time is bursty (event-driven configurations do nothing on most frames), so the mean (cost per second) matters as much as the percentiles |
| `csv_stats.py` | The self-test also checks composite columns against hand-worked values | Every statistic stays reproducible |
| `run_bench.py` | `EXE` = `Build\Windows\SquadQuery\Binaries\Win64\SquadQuery.exe`, launched directly | The real game, so its exit code and kill are reliable (`01` §7) |
| `run_bench.py` | `performance_core_mask()`, `pin_self()`, `applied_mask()` copied unchanged | Every timed run pinned to the performance cores (`01b` B-2) |
| `run_bench.py` | `CONFIGS` holds only `Scripted` in S2 | Each configuration enters with the step that implements it |
| `run_bench.py` | Moves `_agents.csv` and `_events.csv`; `decision_metrics()` computes conflicts per minute, switches per agent per event, oscillations, time-to-cover median and p95 (over agents that reached cover), in-cover-by-next-event and in-cover share | The decision-quality metrics of `01` §6.3 |
| `run_bench.py` | **Already in cover** (the share of agent-events at 0 s) and **time to cover over the agents that moved** (median, p95), besides the overall figures; **stuck at end** (agents whose final distance to their slot is over 75 cm) | *Added after the first S2 run* (`09` S2-a): two thirds of agents kept a slot that stayed valid for the next threat, so the overall median time to cover was 0.00 s and said nothing |
| `run_bench.py` | The **walk ratio**: per agent and event with at least 1 m to go (`MIN_WALK_CM`), time to cover ÷ (distance ÷ walking speed); median and p95 | 1.0 would be a decision on the event frame and a straight walk. The scripted oracle decides on the event frame, so its ratio measures only the detours around blocks and the start-up of character movement; a configuration's ratio above the oracle's is its decision delay (S2 check, `09`) |
| `run_bench.py` | A run fails if its exit code isn't 0, its capture is missing or partial, a report is missing or unreadable, or its log has an `Error:` line | Nothing silently enters a summary |
| `run_bench.py` | `--label` may contain only letters, digits, `_` and `-`; the summary's definition of AI time is generated from `AI_COLUMN` | The label becomes part of `-SQRunTag=`, which the game reads only up to the first space; a definition written out twice had already drifted once (*review*) |
| `run_bench.py` | `STARTUP_MARGIN_SECONDS = 240` | Engine start, navmesh build, spawning and the first-run PSO wait seen in Phase 4 (`09`) |

---

## Part D · S3: squads, the per-agent cover query and the three baselines

S3 builds everything the three per-agent baselines need (`01` §6.2): **B0** polls its own EQS query every 0.5 s ± 0.1 s; **B1** does the same and claims the slot it picks; **B2** queries only when the threat moves (or a claim fails) and claims. The squad registry and the threat context live in the reusable plugin, because SquadQuery (S5) reads the threat the same way; the query, the generator and the brain live in the game module. The squad evaluation itself does not exist yet: **the baselines are captured before any SquadQuery code** (`03`).

### D1. Build rules (changed in S3)

| File · line | What changed, and why |
|---|---|
| `SquadQueryCore.Build.cs`: `PublicDependencyModuleNames … "AIModule"` | `SquadQueryContext_Threat.h` derives from `UEnvQueryContext`, a public header of `AIModule`, so the dependency is public. Smart Objects join in S5 |
| `SquadQuery.Build.cs` | Unchanged: `AIModule` and `SmartObjectsModule` are already public (the generator header derives from `UEnvQueryGenerator_SmartObjects`), and `SquadQueryCore` stays private because no public game header includes a core header (the brain only forward-declares `USquad`) |

### D2. `SquadQueryCore`: `Squad.h`, `SquadMemberComponent.h/.cpp`, `SquadQuerySubsystem.h/.cpp`

| Line | What it does, and why |
|---|---|
| `#define UE_API SQUADQUERYCORE_API` … `#undef UE_API` | The UE 5.6+ convention for exporting members of a `MinimalAPI` class: `UCLASS(MinimalAPI)` exports only what reflection needs, and each function another module calls is marked `UE_API`. Inline getters need no export |
| `UCLASS(MinimalAPI) class USquad : public UObject` | One squad. A UObject because an EQS query needs a UObject owner and the contexts must find the squad from it (`02` §4.1). S3 keeps only its id, members and threat; S5 adds the evaluation state |
| `UPROPERTY(Transient) TArray<TObjectPtr<USquadMemberComponent>> Members;` | GC-visible member list, changed only by the subsystem (`friend class USquadQuerySubsystem`) |
| `HasThreat()`, `GetThreatLocation()` | What the threat context reads |
| `USquadMemberComponent`: `SetSquadId` before `BeginPlay`; `BeginPlay` → `RegisterMember(*this)`; `EndPlay` → `UnregisterMember(*this)` | Makes any pawn a squad member. The director sets the id between `SpawnActorDeferred` and `FinishSpawning`, so registration in `BeginPlay` already knows the squad. Never ticks |
| `UCLASS(MinimalAPI) class USquadQuerySubsystem : public UWorldSubsystem` (S3) | The registry: `RegisterMember` creates a squad on first use (`Squads.FindOrAdd`, `NewObject<USquad>(this)`), `UnregisterMember` removes the member and clears its back pointer, `SetThreat(SquadId, Location)` stores the threat, `FindSquad`, `GetSquads`. Game and PIE worlds only. `Deinitialize` clears every member's squad pointer, so nothing points at a squad the world is destroying. In S5 it becomes tickable and evaluates squads |

### D3. `SquadQueryCore`: `SquadQueryContext_Threat.h/.cpp`, `SquadQueryWork.h/.cpp`

| Line | What it does, and why |
|---|---|
| `static USquad* ResolveSquad(UObject* QueryOwner)` | The squad a query speaks for: the owner itself if it is a `USquad` (the shared query, S5), else the owner actor's member component's squad (a per-agent query). One function, so every configuration resolves the threat the same way |
| `ProvideContext(…)` → `UEnvQueryItemType_Point::SetContextHelper(ContextData, Squad->GetThreatLocation())` | One point, on the floor; the tests add their own heights. No threat → no point, and the tests that need it have nothing to compare against |
| `struct FSquadQueryWork { ItemsGenerated; ItemTests; }` and `FromFinishedQuery(const FEnvQueryInstance&)` | How much work a finished query did, for the "item tests per second" metric. Read from the instance's EQS debug data (`DebugData.OptionStats[…].StepData[…].NumProcessedItems`, `AIModule/Classes/EnvironmentQuery/EnvQueryTypes.h:694-726`): per option, the generator steps count generated items, every later step counts the items its test processed, partial time-sliced steps included. That data exists only under `USE_EQS_DEBUGGER` (every build but Test and Shipping, `EnvQueryTypes.h:33`), so the code is inside `#if USE_EQS_DEBUGGER` and returns zeros otherwise |

### D4. `SQCoverSlotGenerator.h/.cpp`

| Line | What it does, and why |
|---|---|
| `UCLASS(HideDropdown) class USQCoverSlotGenerator : public UEnvQueryGenerator_SmartObjects` | Exactly the engine's Smart Object generator (`01b` #10). Its settings are `protected` with no setters (`SO/Public/EnvQueryGenerator_SmartObjects.h:19-39`), so a subclass sets them from code. `HideDropdown`: not offered in the editor's EQS generator list |
| `Configure(OriginContext, HalfExtent, bOnlyClaimable, bIncludeClaimedSlots)` | Origin context (the querier now; every squad member in S5), box half-size, whether only claimable slots are kept (engine default `true`), and whether the spatial query returns claimed slots (`SmartObjectRequestFilter.bShouldIncludeClaimedSlots`, default `false`, `01b` #18). The per-agent query uses both defaults; the squad query (S5) flips both |

### D5. `SQQueries.h/.cpp`

| Line | What it does, and why |
|---|---|
| `inline const FVector BoxHalfExtentCm(1500.f, 1500.f, 200.f);` | ±15 m × ±15 m × ±2 m around each origin: about three cover blocks each way (`02` §7). `inline` (C++17): one object for every file that includes the header |
| `MakeHiddenFromThreatTest`: `UEnvQueryTest_Trace`, context `USquadQueryContext_Threat`, `GeometryByChannel` on `Visibility`, line, from the context to the item, heights 170 / 90 cm, `Filter`, `BoolValue = true` | "The threat can't see this slot": the trace must hit something. The same channel and heights as the arena's visibility table (`USQArenaSubsystem::ThreatEyeHeightCm`, `SlotProbeHeightCm`), so the AI and the metrics agree on "hidden" |
| `MakeFarFromThreatTest`: `UEnvQueryTest_Distance`, `Distance2D` to the threat, `Filter`, `Minimum`, `USQArenaSubsystem::MinThreatDistanceCm` | "At least 8 m from the threat", the arena's one constant |
| `MakeAgentCoverQuery(Outer)`: generator around `UEnvQueryContext_Querier`; the two filters; `NearMe` = `Distance2D` to the querier, `Score`, `InverseLinear` | The per-agent query: hidden, far enough, and the nearest first. The options and tests are created with the option as their outer, as an asset would own them |
| `Option->Tests[Index]->TestOrder = Index;` | An asset stores each test's order; the manager rebuilds it only if the last test's order is 0 (`EnvQueryManager.cpp:874-884`). Set explicitly, so the order is ours, not inferred |
| (engine) | In `SingleResult` mode the manager moves the most expensive filter (the trace) last and stops at the first item that passes (`01b` #3); the cheap distance filter and the score run on every item first. A competent per-agent implementation, not a strawman |

### D6. `SQAgentBrainComponent` (S3 version)

| Line | What it does, and why |
|---|---|
| `enum class ESQBrainMode { Scripted, PolledEQS, PolledClaims, EventClaims }` | The oracle and B0, B1, B2 |
| `struct FSQBrainCounters`: S2's movement counters plus queries started, completed and aborted, items generated, item tests, keep checks, claim failures, summed query latency | What the run report needs. The director resets it at `BenchStart` (`ResetCounters()`), so the final values are the capture's (*simplify:* the first version copied it at `BenchStart` and subtracted, which needed an `operator-` kept in step with every field) |
| `ConfigureMode(Mode, Query, PollSeed)` | Called after the agent finished spawning: caches the squad (`ResolveSquad`, once; squads live as long as the world), binds the query-finished delegate once (the engine's own EQS nodes keep theirs as a member, `BTTask_RunEQSQuery.cpp:29`), seeds the poll stream and enables ticking for B0 and B1 only |
| `PendingFirstPollDelay` and the first `TickComponent` | A seeded first poll anywhere in one interval, so 128 agents don't poll in step. A newly enabled tick function always runs on the next frame, and a cooldown set before its first tick is ignored (`TickTaskManager.cpp:1681-1691, 2463-2477`), so the first tick only sets the delay and returns. *Found in review:* the first version set the cooldown in `ConfigureMode` and every agent polled on the same frame |
| `TickComponent`: re-arm with 0.5 s ± 0.1 s, then `StartQuery()` | A Behavior Tree service re-arms the same way every time it runs (`AIModule/Private/BehaviorTree/BTService.cpp:105-109`; defaults 0.5 s and 0.1 s at `:20-21`). A poll while the previous query still runs does nothing |
| `OnThreatEvent()` | B2 only: abort the running query (it was asked about the old position) and start a new one. Inside `SquadQueryDecide`: *found in review*, the abort's scan of running queries was first counted as the director's time |
| `StartQuery()`: cheap guards first; `PrepareQueryInstance(Request, SingleResult)` + `RunQuery(Instance, QueryFinishedDelegate)` | Exactly what `FEnvQueryRequest::Execute` does (`EnvQueryManager.cpp:282-286`), except that we keep the instance, whose debug data `FSquadQueryWork` reads when it finishes |
| `AbortQuery()` → `Manager->AbortQuery(RunningQuery->QueryID)` | The engine calls the finish delegate at once with an aborted result (`EnvQueryManager.cpp:408-428`) |
| `OnQueryFinished(Result)` | Inside `SquadQueryDecide`: the delegate runs inside the EQS tick, but outside the EQS budget (`01b` #5). Adds the query's work, counts it as completed (with its latency) or aborted (without: an aborted query was not waited for, *review*), then: no result → keep the current target; the best slot → `FindSlotIndex` (one hash lookup, `05` B4); the same slot, or `ShouldKeepCurrent` → nothing; else, for B1/B2, `MarkSlotAsClaimed(Handle, Normal)`: on failure count it and query again next frame (`01b` #8); on success free the old claim and keep the new one; then `SetTarget` |
| `ShouldKeepCurrent(NewSlot)` | The keep-current rule of `02` §7: a valid slot is left only for one at least 3 m closer. With claims, the agent's own slot is never in its own results (it is claimed, `01b` #8), so its validity must be checked: the distance comparison first (no trace if the new slot is more than 3 m closer, *simplify*), then the 8 m rule and one Visibility trace from the threat, both through the arena's shared helpers (`USQArenaSubsystem::IsFarFromThreat`, `TraceHidden`, *simplify*). The trace is counted as **one item test**, so the work comparison stays honest |
| `ReleaseClaim()` | `MarkSlotAsFree(Claim)`, then forget the handle. Also in `EndPlay`: a claim must not outlive its holder (`01b` #7) |
| `EndPlay` → `SilentlyRemoveAllQueriesByQuerier(*GetOwner())` | No query may call back into a component that is going away |
| S2's `SetTarget`, `RequestMove`, `OnMoveFinished`, `RetryMove` | Unchanged: every configuration ends here (`05` C3) |

### D7. Director, agent and arena (changed in S3)

| Line | What changed, and why |
|---|---|
| `SQBench::Modes[]` and `ParseMode` | `-SQMode=Scripted|B0|B1|B2` → `ESQBrainMode`, from one table; the error message lists the names from the same table (*simplify*) |
| `AgentQuery = SQQueries::MakeAgentCoverQuery(*this);` | One query template for every agent, a `UPROPERTY` of the director. The EQS manager caches templates, not results (`01b` #4), so every query of the run uses the same cached template |
| `Agent->GetSquadMember()->SetSquadId(SquadIndex)` before `FinishSpawning`; `ConfigureMode(…, PollSeed)` after | The member registers in `BeginPlay`; the brain needs its registered squad and tick function. The poll seed is `HashCombineFast(seed, agent index)`: per agent, and fixed by the run's seed |
| `FireThreatEvent`: `SetThreat(SquadId, ThreatLocation)` for every squad, then `OnThreatEvent()` on every brain (B2) or the scripted assignment | Every configuration learns of the threat the same way (`01` §6.2) |
| `ResetCounters()` at `BenchStart`; the report's squad from `GetSquadMember()->GetSquadId()` | One squad id (the member's) instead of a copy in the brain (*simplify*) |
| The agents report: `queries_started, queries_finished, queries_aborted, items_generated, item_tests, keep_checks, claim_failures, query_latency_s` | The query work per agent over the capture |
| Header field `eqsdebug=` | 1 if the item counts are real: the EQS debugger is compiled in **and** `FEnvQueryInstance::bDebuggingInfoEnabled` is on (`EnvQueryTypes.h:1317`). The script leaves item counts out otherwise (*review*) |
| `ASQAgent`: `UPROPERTY(VisibleAnywhere) TObjectPtr<USquadMemberComponent> SquadMember` | Every agent is a squad member in every configuration; the baselines read their threat through it |
| `USQArenaSubsystem::FindSlotIndex(Handle)` and `TMap<FSmartObjectSlotHandle, int32> SlotIndexByHandle` | EQS returns slot handles; the table works in indices. Filled with the handles once (`FillSlotHandles`); the handle type has its own `GetTypeHash` (`SO/Public/SmartObjectTypes.h:254-257`) |
| `IsFarFromThreat`, `TraceHidden` (static) | The cover rule's two parts, now shared by the visibility table, `IsValidCover` and the baselines' keep-current check (*simplify*) |

### D8. `Tools\run_bench.py` (changed in S3)

| Change | Why |
|---|---|
| `CONFIGS`: `B0`, `B1`, `B2` | The baselines enter with the step that implements them |
| `decision_metrics`: queries per second, aborted queries, item tests and items generated per second (only when `eqsdebug=1`), claim failures per minute, keep checks, mean query latency (completed queries) | H1 (work), H3 (polling repeats work), H5 (claim races) need them |
| Summary: `Queries / s`, `Item tests / s` in the cost table; `Claim failures / min` in the decision table | The numbers the baselines are compared on |
| A `total(column)` helper and one `stat`/`metric` helper per table | The same sum and lookup were written out a dozen times (*simplify*) |
