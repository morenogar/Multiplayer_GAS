// Fill out your copyright notice in the Description page of Project Settings.

#include "ViperMapPushLibrary.h"

#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString UViperMapPushLibrary::PushMapJson(const FString& MapJson)
{
	if (MapJson.IsEmpty())
	{
		return TEXT("Nothing to build: the map was empty.");
	}

	// Through a file rather than as an argument. The console takes one string, and a map is
	// several kilobytes of JSON full of quotes and braces: escaping that into a py command is a
	// quoting problem with no upside, and the file is also what you look at when it goes wrong.
	const FString PushPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("ViperMapPush.json"));

	if (!FFileHelper::SaveStringToFile(MapJson, *PushPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return FString::Printf(TEXT("Could not write %s"), *PushPath);
	}

	const FString ScriptPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectContentDir() / TEXT("Python/viper_map_push.py"));

	if (!IFileManager::Get().FileExists(*ScriptPath))
	{
		return FString::Printf(TEXT("The map was saved to %s, but %s is missing"),
			*PushPath, *ScriptPath);
	}

	if (!GEngine)
	{
		return TEXT("No engine to run the build on.");
	}

	// The py console command, so this module needs no dependency on the Python plugin: it is
	// either there and answers, or it is not and says so in the log.
	const FString Command = FString::Printf(TEXT("py \"%s\""), *ScriptPath);
	GEngine->Exec(nullptr, *Command);

	return FString::Printf(
		TEXT("Pushed %d characters to %s and ran the builder. The Output Log has what it built, ")
		TEXT("and the navmesh still needs rebuilding (Build > Build Paths)."),
		MapJson.Len(), *PushPath);
}
