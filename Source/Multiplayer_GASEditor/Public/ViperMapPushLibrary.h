// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ViperMapPushLibrary.generated.h"

/**
 * The door the browser lab pushes a map through.
 *
 * Remote Control is the only way into a running editor from a web page: it answers on 30010 with
 * Access-Control-Allow-Origin, and it calls UFUNCTIONs. The map builder is Python, which Remote
 * Control cannot reach, so this sits between them - it takes the JSON, puts it next to the
 * project, and runs the one Python file that does the building. Keeping the builder in one place
 * matters more than keeping the path short.
 *
 * Called like this, from a page opened with file:// so the browser allows plain http:
 *
 *   PUT http://127.0.0.1:30010/remote/object/call
 *   { "objectPath": "/Script/Multiplayer_GASEditor.Default__ViperMapPushLibrary",
 *     "functionName": "PushMapJson",
 *     "parameters": { "MapJson": "{...the lab's Copy for Unreal JSON...}" } }
 */
UCLASS()
class MULTIPLAYER_GASEDITOR_API UViperMapPushLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Builds the given map in the level that is currently open.
	 *
	 * @param MapJson The lab's "Copy for Unreal" JSON.
	 * @return What it did, or why it could not.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Viper|Test")
	static FString PushMapJson(const FString& MapJson);
};
