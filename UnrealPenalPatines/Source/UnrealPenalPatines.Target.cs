using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealPenalPatinesTarget : TargetRules
{
	public UnrealPenalPatinesTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PenalPatines");
	}
}
