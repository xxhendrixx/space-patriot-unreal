using UnrealBuildTool;

public class SpacePatriotUnreal : ModuleRules
{
    public SpacePatriotUnreal(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "JsonUtilities", "ProceduralMeshComponent" });
    }
}
