using UnrealBuildTool;
using System.Collections.Generic;

public class Multiplayer_GASEditorTarget : TargetRules
{
    public Multiplayer_GASEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;

        DefaultBuildSettings = BuildSettingsVersion.V7; 
        IncludeOrderVersion  = EngineIncludeOrderVersion.Latest;
        
        bOverrideBuildEnvironment = true;

        ExtraModuleNames.Add("Multiplayer_GAS");
        ExtraModuleNames.Add("Multiplayer_GASEditor");
    }
}