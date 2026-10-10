using UnrealBuildTool;

public class PenalPatines : ModuleRules
{
	public PenalPatines(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageType.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore"
		});
	}
}
