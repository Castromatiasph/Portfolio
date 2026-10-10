using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealPenalPatinesEditorTarget : TargetRules
{
	public UnrealPenalPatinesEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PenalPatines");
	}
}
