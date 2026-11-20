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

		// The area trigger's shape drawing (scene proxy).
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"RenderCore",
			}
		);
	}
}
