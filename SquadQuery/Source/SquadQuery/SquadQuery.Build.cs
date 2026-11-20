// P07 SquadQuery - primary game module: test arena, demo agents, per-agent EQS baselines and benchmark harness only.
// The reusable system lives in the SquadQueryCore plugin; this module depends on it, never the reverse.

using UnrealBuildTool;

public class SquadQuery : ModuleRules
{
	public SquadQuery(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// Private for now: no public header of this module includes a plugin header yet (Phase 4 has no classes).
		// It becomes public in the first step where a public header needs a SquadQueryCore type (03_Implementation_Roadmap.md).
		PrivateDependencyModuleNames.AddRange(new string[] { "SquadQueryCore" });
	}
}
