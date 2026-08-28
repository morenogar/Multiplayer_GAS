#include "Multiplayer_GASEditorModule.h"

#include "AssetToolsModule.h"
#include "FAssetTA_ViperInteractResponse.h"
#include "IAssetTools.h"

void FMultiplayer_GASEditorModule::StartupModule()
{
    IAssetTools& AssetTools =
     FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    EAssetTypeCategories::Type Category =
        AssetTools.RegisterAdvancedAssetCategory(
            FName("Viper"),
            FText::FromString("Viper")
        );

    TSharedRef<IAssetTypeActions> Action =
        MakeShared<FAssetTA_ViperInteractResponse>(Category);

    AssetTools.RegisterAssetTypeActions(Action);
    
    RegisteredActions.Add(Action);
}

void FMultiplayer_GASEditorModule::ShutdownModule()
{
    if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
    {
        IAssetTools& AssetTools =
            FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

        for (auto& Action : RegisteredActions)
        {
            AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
        }
    }

    RegisteredActions.Empty();
}
