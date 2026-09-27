using UnrealBuildTool;

public class SpacePatriotUnreal : ModuleRules
{
    public SpacePatriotUnreal(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // The port's source files have independent anonymous-namespace JSON helpers.
        // Unity amalgamation would combine those translation units and redefine them.
        bUseUnity = false;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "JsonUtilities", "ProceduralMeshComponent", "UMG", "Slate", "SlateCore" });
    }
}
