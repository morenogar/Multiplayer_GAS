using UnrealBuildTool;

public class Multiplayer_GASEditor : ModuleRules
{
    public Multiplayer_GASEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore", 
            "AssetTools", // Required for IAssetTypeActions
            "UnrealEd",   // Required for UFactory
            "Multiplayer_GAS" // Your runtime module
        });

        // UViperStateTreeEditLibrary: edits StateTree assets (states, conditions, bindings).
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "StateTreeModule",
            "StateTreeEditorModule",
            "PropertyBindingUtils",
            "GameplayTags",
            "Json"
        });
    }
}