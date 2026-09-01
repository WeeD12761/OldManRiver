using UnrealBuildTool;
using System.IO;


public class Igloo : ModuleRules
{

    private string ModulePath
    {
        get { return ModuleDirectory; }
    }

	public Igloo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PrivateIncludePaths.AddRange(
            new string[] {
				"Igloo/Private",
				Path.Combine(Path.GetFullPath(Target.RelativeEnginePath), @"Source/Runtime/Renderer/Private"),
			}
		);
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(new string[]
			{
				"CoreUObject",
				"Slate",
				"SlateCore",
				"RHI",
				"Renderer",
				"RenderCore",
				"OSC"
			}
		);
	}
}
