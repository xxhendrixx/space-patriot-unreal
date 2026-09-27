using UnrealBuildTool;

public class SpacePatriotBlueprintTools : ModuleRules
{
    public SpacePatriotBlueprintTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "SpacePatriotUnreal",
            "UnrealEd", "BlueprintGraph", "Kismet", "KismetCompiler"
        });
    }
}
