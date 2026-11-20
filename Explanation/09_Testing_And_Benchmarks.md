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

## Phases 5–8 (to be written)

- S1: the runtime navmesh and runtime Smart Objects in a cooked build (the riskiest assumption, `03`).
- S2: harness and metric checks; S3: the baseline capture; S4: solver tests; S5: SQ checks; S7: the headline matrix and SC1–SC7.
