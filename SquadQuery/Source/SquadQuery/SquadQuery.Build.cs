// P07 SquadQuery - primary game module: test arena, demo agents, per-agent EQS baselines and benchmark harness only.
// The reusable system lives in the SquadQueryCore plugin; this module depends on it, never the reverse.

using UnrealBuildTool;

public class SquadQuery : ModuleRules
{
	public SquadQuery(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// SmartObjectsModule is public: SQCoverBlock.h derives from USmartObjectBehaviorDefinition and
		// SQArenaSubsystem.h stores FSmartObjectSlotHandle (both public headers).
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "SmartObjectsModule" });

		// Private: no public header of this module includes a SquadQueryCore header yet; it becomes public in the first
		// step where one does (03_Implementation_Roadmap.md). NavigationSystem is used only in SQArenaSubsystem.cpp.
		// GameplayTags: the Smart Object request filter and slot definition hold tag containers and queries, whose
		// constructors and copies the arena's code instantiates, so this module links against GameplayTags itself.
		PrivateDependencyModuleNames.AddRange(new string[] { "SquadQueryCore", "NavigationSystem", "GameplayTags" });
	}
}
