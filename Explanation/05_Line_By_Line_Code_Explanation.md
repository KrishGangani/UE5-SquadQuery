# 05 · Line By Line Code Explanation: SquadQuery

**Rule:** every file that exists in `SquadQuery\` (and every script in `Tools\`) is explained here, line by line, in the same step it is written. Sections follow the build order in [03_Implementation_Roadmap.md](03_Implementation_Roadmap.md).

| Part | Step | Files |
|---|---|---|
| A | Phase 4, F1–F3 (foundation) | `.uproject`, two `Target.cs`, two `Build.cs`, two module `.cpp`, `.uplugin`, three config `.ini`, `Tools\*.bat` |
| B | S1 onwards | written with the code |

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
