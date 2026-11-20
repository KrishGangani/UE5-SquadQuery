// P07 SquadQuery - game target (standalone/packaged builds).

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
