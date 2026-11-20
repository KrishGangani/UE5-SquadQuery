# 04 · C++ Fundamentals: SquadQuery

**Rule:** only the C++ (and build-language) ideas this project's code actually relies on, each with a tiny example first and then where it appears in our code. This file grows with every step. Phase 4 has two source files of one line each, so it starts small.

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
