// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "WorldLockingToolsModule.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogWLT)

WorldLockingTools::FWorldLockingToolsModule* UWorldLockingToolsFunctionLibrary::GetWorldLockingToolsModule()
{
	return FModuleManager::GetModulePtr<WorldLockingTools::FWorldLockingToolsModule>("WorldLockingTools");
}

bool UWorldLockingToolsFunctionLibrary::StartWorldLockingTools(FWorldLockingToolsConfiguration Configuration)
{
#if defined(USING_FROZEN_WORLD)
	WorldLockingTools::FWorldLockingToolsModule* WLTModule = GetWorldLockingToolsModule();
	if (WLTModule == nullptr || WLTModule->FrozenWorldPlugin == nullptr)
	{
		return false;
	}

	WLTModule->FrozenWorldPlugin->Start(Configuration);
	return true;
#else
	return true;
#endif
}

void UWorldLockingToolsFunctionLibrary::StopWorldLockingTools()
{
#if defined(USING_FROZEN_WORLD)
	WorldLockingTools::FWorldLockingToolsModule* WLTModule = GetWorldLockingToolsModule();
	if (WLTModule == nullptr || WLTModule->FrozenWorldPlugin == nullptr)
	{
		return;
	}

	WLTModule->FrozenWorldPlugin->Stop();
#endif
}

void UWorldLockingToolsFunctionLibrary::SaveAsync()
{
#if defined(USING_FROZEN_WORLD)
	WorldLockingTools::FWorldLockingToolsModule* WLTModule = GetWorldLockingToolsModule();
	if (WLTModule == nullptr || WLTModule->FrozenWorldPlugin == nullptr)
	{
		return;
	}

	WLTModule->FrozenWorldPlugin->SaveAsync();
#endif
}

void UWorldLockingToolsFunctionLibrary::LoadAsync()
{
#if defined(USING_FROZEN_WORLD)
	WorldLockingTools::FWorldLockingToolsModule* WLTModule = GetWorldLockingToolsModule();
	if (WLTModule == nullptr || WLTModule->FrozenWorldPlugin == nullptr)
	{
		return;
	}

	WLTModule->FrozenWorldPlugin->LoadAsync();
#endif
}

void UWorldLockingToolsFunctionLibrary::Reset()
{
#if defined(USING_FROZEN_WORLD)
	WorldLockingTools::FWorldLockingToolsModule* WLTModule = GetWorldLockingToolsModule();
	if (WLTModule == nullptr || WLTModule->FrozenWorldPlugin == nullptr)
	{
		return;
	}

	WLTModule->FrozenWorldPlugin->Reset();
#endif
}

IMPLEMENT_MODULE(WorldLockingTools::FWorldLockingToolsModule, WorldLockingTools)