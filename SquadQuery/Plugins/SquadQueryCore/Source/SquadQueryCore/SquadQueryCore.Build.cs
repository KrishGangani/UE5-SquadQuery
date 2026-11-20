// SquadQueryCore - the reusable runtime module (shared squad evaluation, assignment solver, event policy, telemetry).
// It must not depend on any game module.

using UnrealBuildTool;

public class SquadQueryCore : ModuleRules
{
	public SquadQueryCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Phase 4: the engine basics only. AIModule (EQS), SmartObjectsModule (claims) and GameplayTags are added
		// in the implementation step that first includes their headers (03_Implementation_Roadmap.md).
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
