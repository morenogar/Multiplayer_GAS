// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"


class IAssetTypeActions;

class MULTIPLAYER_GASEDITOR_API FMultiplayer_GASEditorModule : public IModuleInterface
{
public:
	// Begin IModuleInterface

	virtual void StartupModule() override;
	
	virtual void ShutdownModule() override;
	
	TArray<TSharedPtr<IAssetTypeActions>> RegisteredActions;
};
