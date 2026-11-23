using UnrealBuildTool;

public class BangoTriggersEditor : ModuleRules
{
	public BangoTriggersEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BangoTriggers",
				"UnrealEd",
				"InputCore",
				"Slate",
				"SlateCore",
				"PropertyEditor",
			}
		);
	}
}
