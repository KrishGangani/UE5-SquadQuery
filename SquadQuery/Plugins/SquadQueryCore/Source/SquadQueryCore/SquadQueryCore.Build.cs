// SquadQueryCore - the reusable runtime module (shared squad evaluation, assignment solver, event policy, telemetry).
// It must not depend on any game module.

using UnrealBuildTool;

public class SquadQueryCore : ModuleRules
{
	public SquadQueryCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// AIModule is public from S3: SquadQueryContext_Threat.h derives from UEnvQueryContext (a public header).
		// SmartObjectsModule (claims, S5) and GameplayTags are added in the step that first includes their headers
		// (03_Implementation_Roadmap.md).
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AIModule" });
	}
}
