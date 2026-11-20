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

The APIs of the system itself (EQS, Smart Objects, world subsystems, navigation, CSV profiler) are researched in `01b` and enter this file from S1.

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
