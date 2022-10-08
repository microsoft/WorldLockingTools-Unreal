// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "FrozenWorldPlugin.h"

#include "WorldLockingToolsTypes.h"
#include "WorldLockingToolsModule.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWLT, All, All);

namespace WorldLockingTools
{
	class FWorldLockingToolsModule : public IModuleInterface
	{
	public:
		void StartupModule() override
		{
#if defined(USING_FROZEN_WORLD)
			FrozenWorldPlugin = MakeShared<FFrozenWorldPlugin>();
			FrozenWorldPlugin->Register();
#endif
		}

		void ShutdownModule() override
		{
#if defined(USING_FROZEN_WORLD)
			FrozenWorldPlugin->Unregister();
#endif
		}

		TSharedPtr<FFrozenWorldPlugin> FrozenWorldPlugin;
	};
}

UCLASS(ClassGroup = WorldLockingTools)
class WORLDLOCKINGTOOLS_API UWorldLockingToolsFunctionLibrary :
	public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
private:
	static WorldLockingTools::FWorldLockingToolsModule* GetWorldLockingToolsModule();

public:

	// Enable world locking tools with the input settings.
	UFUNCTION(BlueprintCallable, Category = "World Locking Tools")
	static bool StartWorldLockingTools(FWorldLockingToolsConfiguration Configuration);

	// Disable world locking tools.  Adjustment and pinning transforms will be unchanged, but will not update until WLT is started again.
	UFUNCTION(BlueprintCallable, Category = "World Locking Tools")
	static void StopWorldLockingTools();

	// Trigger a manual save of the Frozen World data.
	// This must be called if WLT is started with AutoSave = false.
	UFUNCTION(BlueprintCallable, Category = "World Locking Tools")
	static void SaveAsync();

	// Trigger a manual load of the Frozen World data.  
	// This must be called if WLT is started with AutoLoad = false.
	UFUNCTION(BlueprintCallable, Category = "World Locking Tools")
	static void LoadAsync();

	// Reset WorldLocking to a well-defined, empty state
	UFUNCTION(BlueprintCallable, Category = "World Locking Tools")
	static void Reset();
};
