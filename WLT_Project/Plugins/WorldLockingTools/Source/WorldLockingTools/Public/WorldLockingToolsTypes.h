// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "WorldLockingToolsTypes.generated.h"

/*Configuration for World Locking Tools.*/
USTRUCT(BlueprintType, Category = "World Locking Tools")
struct FWorldLockingToolsConfiguration
{
	GENERATED_BODY()

	/*Whether to automatically load frozen world data at startup.  If false, data must be manually loaded.*/
	UPROPERTY(BlueprintReadWrite, Category = "World Locking Tools")
	bool AutoLoad = true;

	/*Whether to automatically save frozen world data periodically.  If false, data must be manually saved.*/
	UPROPERTY(BlueprintReadWrite, Category = "World Locking Tools")
	bool AutoSave = true;

	/*Time in seconds between autosaves.*/
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = "World Locking Tools")
	float AutoSaveInterval = 10;

	/*True to automatically perform refreezes*/
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = "World Locking Tools")
	bool AutoRefreeze = true;

	/*True to automatically perform merges*/
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = "World Locking Tools")
	bool AutoMerge = true;

	/*
	* Zero out pitch and roll from FrozenWorldEngine's computed correction.
	* This does not affect pitch and roll from the AlignmentManager (SpacePins).
	*/
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = "World Locking Tools")
	bool NoPitchAndRoll = false;

	/*Minimum distance that can occur in regular anchor creation.*/
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = "World Locking Tools")
	float MinNewAnchorDistance = 100.0f;

	/*Maximum distance to be considered for creating edges to new anchors*/
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = "World Locking Tools")
	float MaxAnchorEdgeLength = 120.0f;

	/*
	* Maximum number of local anchors in the internal anchor graph.
	* Zero or negative means unlimited anchors.
	*/
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = "World Locking Tools")
	int MaxLocalAnchors = 0;
};