// SquadQueryCore - the reusable runtime module (shared squad evaluation, assignment solver, event policy, telemetry).
// It must not depend on any game module.

using UnrealBuildTool;

public class SquadQueryCore : ModuleRules
{
	public SquadQueryCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// AIModule is public from S3: SquadQueryContext_Threat.h derives from UEnvQueryContext (a public header).
		// SmartObjectsModule is public from S5: SquadMemberComponent.h holds a claim handle (SmartObjectRuntime.h).
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AIModule", "SmartObjectsModule" });
	}
}
