using UnrealBuildTool;

public class BangoTriggers : ModuleRules
{
	public BangoTriggers(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public: trigger headers use Bango types (conditions, script handles, values).
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"BangoScripts",
			}
		);

		// The regions' drawing in the editor (a scene proxy).
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"RenderCore",
			}
		);
	}
}
