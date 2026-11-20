// P07 SquadQuery - editor target.

using UnrealBuildTool;

public class SquadQueryEditorTarget : TargetRules
{
	public SquadQueryEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("SquadQuery");
	}
}
