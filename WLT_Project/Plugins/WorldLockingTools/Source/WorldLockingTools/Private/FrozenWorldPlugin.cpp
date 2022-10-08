// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "FrozenWorldPlugin.h"

#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Kismet/GameplayStatics.h"

#include "Triangulator.h"
#include "FrozenWorldPoseExtensions.h"
#include "WorldLockingToolsModule.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameDelegates.h"
#include "HAL/PlatformFileManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace WorldLockingTools
{
	FFrozenWorldPlugin* FFrozenWorldPlugin::Get()
	{
		return &IModularFeatures::Get().GetModularFeature<FFrozenWorldPlugin>(GetModularFeatureName());
	}

	void FFrozenWorldPlugin::Register()
	{
		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);

		LastSavingTime = std::numeric_limits<float>::min();

		frozenWorldFile = "frozenWorldState.hkfw";
		stateFileNameBase = FPlatformProcess::UserDir() / frozenWorldFile;

		FrozenWorldInterop.LoadFrozenWorld();
		FrozenWorldInterop.FW_Init();
	}

	void FFrozenWorldPlugin::OnEndPlay(UWorld* InWorld)
	{
		Stop();
	}

	void FFrozenWorldPlugin::Start(FWorldLockingToolsConfiguration Configuration)
	{
		// Apply Configuration Data
		AutoLoad = Configuration.AutoLoad;
		AutoSave = Configuration.AutoSave;
		AutoSaveInterval = Configuration.AutoSaveInterval;
		AutoRefreeze = Configuration.AutoRefreeze;
		AutoMerge = Configuration.AutoMerge;
		NoPitchAndRoll = Configuration.NoPitchAndRoll;
		FrozenWorldAnchorManager.MinNewAnchorDistance = Configuration.MinNewAnchorDistance;
		FrozenWorldAnchorManager.MaxAnchorEdgeLength = Configuration.MaxAnchorEdgeLength;
		FrozenWorldAnchorManager.MaxLocalAnchors = Configuration.MaxLocalAnchors;

		Enabled = true;

		if (initializationState != InitializationState::Running)
		{
			//TODO: this is still updated when switching maps, causing a nullref.
			// Change to IOpenXRExtension
			FCoreDelegates::OnBeginFrame.AddRaw(this, &FFrozenWorldPlugin::Update);
			//FGameDelegates::Get().GetEndPlayMapDelegate().AddRaw(this, &FFrozenWorldPlugin::OnEndPlay);
			FWorldDelegates::OnWorldBeginTearDown.AddRaw(this, &FFrozenWorldPlugin::OnEndPlay);

#if WITH_EDITOR
			// When PIE stops (when remoting), ShutdownModule is not called.
			FEditorDelegates::EndPIE.AddRaw(this, &FFrozenWorldPlugin::HandleEndPIE);
#endif

			CacheCameraHierarchy();

			initializationState = InitializationState::Starting;

			if (AutoLoad)
			{
				LoadAsync();
			}
			else
			{
				Reset();
				initializationState = InitializationState::Running;
			}
		}
	}

	void FFrozenWorldPlugin::Stop()
	{
		FCoreDelegates::OnBeginFrame.RemoveAll(this);
		FWorldDelegates::OnWorldBeginTearDown.RemoveAll(this);

#if WITH_EDITOR
		FEditorDelegates::EndPIE.RemoveAll(this);
#endif

		Enabled = false;
		initializationState = InitializationState::Uninitialized;
	}

	void FFrozenWorldPlugin::HandleEndPIE(const bool InIsSimulating)
	{
		Reset();
	}

	bool FFrozenWorldPlugin::CacheCameraHierarchy()
	{
		if (!GWorld)
		{
			return false;
		}

		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GWorld, 0);
		if (PlayerPawn == nullptr)
		{
			return false;
		}

		if (CameraParent == nullptr)
		{
			UCameraComponent* PlayerCamera = PlayerPawn->FindComponentByClass<UCameraComponent>();
			if (PlayerCamera == nullptr)
			{
				return false;
			}
			CameraParent = PlayerCamera->GetAttachParent();
		}

		if (CameraParent == nullptr)
		{
			UE_LOG(LogWLT, Error, TEXT("Camera must have a parent component."));
			return false;
		}

		if (AdjustmentFrame == nullptr)
		{
			AdjustmentFrame = CameraParent->GetAttachParent();
		}

		if (AdjustmentFrame == nullptr)
		{
			UE_LOG(LogWLT, Error, TEXT("Camera must have a grandparent component."));
			return false;
		}

		if (AdjustmentFrame->GetAttachParent() == nullptr)
		{
			UE_LOG(LogWLT, Error, TEXT("Camera's grandparent component cannot be the Pawn's DefaultSceneRoot."));
			return false;
		}

		return true;
	}

	void FFrozenWorldPlugin::Unregister()
	{
		FCoreDelegates::OnBeginFrame.RemoveAll(this);
		FrozenWorldInterop.FW_Destroy();

		IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
	}

	FTransform FFrozenWorldPlugin::FrozenFromSpongy()
	{
		return FFrozenWorldPoseExtensions::Multiply(FrozenFromLocked(), LockedFromSpongy());
	}

	FTransform FFrozenWorldPlugin::SpongyFromFrozen()
	{
		return FFrozenWorldPoseExtensions::Inverse(FrozenFromSpongy());
	}

	FTransform FFrozenWorldPlugin::PlayspaceFromSpongy()
	{
		if (CameraParent == nullptr)
		{
			CacheCameraHierarchy();
		}

		if (CameraParent != nullptr)
		{
			return CameraParent->GetRelativeTransform();
		}

		return FTransform::Identity;
	}

	FTransform FFrozenWorldPlugin::FrozenFromLocked()
	{
		return FFrozenWorldPoseExtensions::Multiply(FrozenFromPinned(), pinnedFromLocked);
	}

	FTransform FFrozenWorldPlugin::LockedFromFrozen()
	{
		return FFrozenWorldPoseExtensions::Inverse(FrozenFromLocked());
	}

	FTransform FFrozenWorldPlugin::FrozenFromPinned()
	{
		if (AdjustmentFrame == nullptr)
		{
			CacheCameraHierarchy();
		}

		if (AdjustmentFrame == nullptr)
		{
			return FTransform::Identity;
		}

		if (AdjustmentFrame->GetAttachParent() != nullptr)
		{
			return AdjustmentFrame->GetAttachParent()->GetComponentTransform();
		}

		return FTransform::Identity;
	}

	FTransform FFrozenWorldPlugin::PinnedFromLocked()
	{
		return pinnedFromLocked;
	}

	FTransform FFrozenWorldPlugin::PinnedFromFrozen()
	{
		return FFrozenWorldPoseExtensions::Inverse(FrozenFromPinned());
	}

	FTransform FFrozenWorldPlugin::LockedFromSpongy()
	{
		return FFrozenWorldPoseExtensions::Multiply(lockedFromPlayspace, PlayspaceFromSpongy());
	}

	void FFrozenWorldPlugin::Update()
	{
		if (CameraParent == nullptr || AdjustmentFrame == nullptr)
		{
			CacheCameraHierarchy();
			return;
		}

		if (initializationState != InitializationState::Running)
		{
			return;
		}

		if (hasPendingLoadTask)
		{
			return;
		}

		// FAnchorManager::Update takes care of creating anchors&edges and feeding the up-to-date state
		// into the FrozenWorld engine
		if (!FrozenWorldAnchorManager.Update())
		{
			// No spongy anchors.
			//FragmentManager.Pause() will set all fragments to disconnected.
			FrozenWorldFragmentManager.Pause();
			return;
		}

		// The basic output from the FrozenWorld engine (current fragment and its alignment)
		// are applied to the Unreal scene
		FrozenWorldFragmentManager.Update(AutoRefreeze, AutoMerge);

		/// The following assumes a camera hierarchy like this:
		/// Nodes_A => AdjustmentFrame => Nodes_B => camera
		/// The cumulative effect of Nodes_B is to transform from Spongy space to playspace.
		/// Spongy space is the space that the camera moves about in, and is the space that
		/// coordinates coming from scene agnostic APIs like XR are in.
		/// (Note spongy space is the same as tracking space.  Many Unreal APIs are in Unreal world space)
		/// The internal structure of that graph is inconsequential here, the only dependency
		/// is on the cumulative transform, PlayspaceFromSpongy.
		/// Likewise, the cumulative effect of Nodes_A is to transform from alignment space (described below)
		/// to Unreal's world space, referred to here as FrozenSpace.
		/// The AdjustmentFrame's transform is composed of two transforms. 
		/// The first comes from the FrozenWorld engine DLL as the inverse of Plugin.GetAlignment(), 
		/// and transforms from Playspace to the base stable world locked space, labeled as
		/// LockedFromPlayspace.
		/// The second transforms from this stable but arbitrary space to a space locked
		/// to a finite set of real world markers. This transform is labeled PinnedFromLocked.
		/// The transform chain equivalent of the above camera hierarchy is:
		/// FrozenFromPinned * [PinnedFromLocked * LockedFromPlayspace] * PlayspaceFromSpongy * SpongyFromCamera
		/// 
		/// FrozenFromSpongy and its inverse are useful for converting between the coordinates of scene agnostic APIs (e.g. XR)
		/// and Frozen coordinates, i.e. Unreal's global space.
		/// FrozenFromLocked is convenient for converting between the "frozen" coordinates of the FrozenWorld engine DLL
		/// and Unreal's global space, i.e. Frozen coordinate.
		if (Enabled)
		{
			FTransform playspaceFromLocked = FrozenWorldInterop.GetAlignment();
			if (NoPitchAndRoll)
			{
				// Zero out pitch and roll from frozen world engine
				FRotator rotation = playspaceFromLocked.GetRotation().Rotator();
				rotation.Pitch = 0;
				rotation.Roll = 0;
				playspaceFromLocked.SetRotation(FQuat(rotation));
			}
			lockedFromPlayspace = FFrozenWorldPoseExtensions::Inverse(playspaceFromLocked);

			spongyFromCamera = FrozenWorldInterop.GetSpongyHead();

			FTransform lockedHeadPose = FFrozenWorldPoseExtensions::Multiply(lockedFromPlayspace, 
				FFrozenWorldPoseExtensions::Multiply(PlayspaceFromSpongy(), spongyFromCamera));
			FrozenWorldAlignmentManager.ComputePinnedPose(lockedHeadPose);
			pinnedFromLocked = FrozenWorldAlignmentManager.PinnedFromLocked;
		}
		else
		{
			FRotator DeviceOrientation;
			FVector DevicePosition;
			UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(DeviceOrientation, DevicePosition);

			spongyFromCamera = FTransform(DeviceOrientation, DevicePosition);
			// Note leave adjustment and pinning transforms alone, to facilitate
			// comparison of behavior when toggling FW enabled.
		}

		if (GWorld == nullptr)
		{
			return;
		}

		if (AdjustmentFrame == nullptr)
		{
			CacheCameraHierarchy();
		}

		if (AdjustmentFrame != nullptr)
		{	
			FTransform NewTransform = FFrozenWorldPoseExtensions::Multiply(pinnedFromLocked, lockedFromPlayspace);
			AdjustmentFrame->SetRelativeTransform(NewTransform);
		}

		if (AutoSave && GWorld->RealTimeSeconds >= LastSavingTime + AutoSaveInterval)
		{
			SaveAsync();
		}
	}

	void FFrozenWorldPlugin::Reset()
	{
		FrozenWorldAnchorManager.Reset();
		FrozenWorldFragmentManager.Reset();
		FrozenWorldAlignmentManager.ClearAlignmentAnchors();
		FrozenWorldAlignmentManager.SendAlignmentAnchors();

		FrozenWorldInterop.ClearFrozenAnchors();
		FrozenWorldInterop.ResetAlignment(FTransform::Identity);

		CameraParent = nullptr;
		AdjustmentFrame = nullptr;
	}

	void FFrozenWorldPlugin::LoadAsync()
	{
		if (HasPendingIO())
		{
			return;
		}

		TSharedPtr<BackgroundOperation> LoadTask = MakeShared<BackgroundOperation>();
		LoadTask->QueueBackgroundTask([WeakThis{ TWeakPtr<FFrozenWorldPlugin, ESPMode::ThreadSafe>(AsShared()) }, LoadTask]()
		{
			if (TSharedPtr<FFrozenWorldPlugin, ESPMode::ThreadSafe> This = WeakThis.Pin())
			{
				This->hasPendingLoadTask = true;

				This->Reset();

				FString tryFileNames[] = { This->stateFileNameBase, This->stateFileNameBase + ".old" };

				for (FString fileName : tryFileNames)
				{
					if (FPaths::FileExists(fileName))
					{
						FrozenWorld_Deserialize_Stream ps;
						ps.includePersistent = true;
						ps.includeTransient = false;
						This->FrozenWorldInterop.DeserializeOpen(&ps);

						const int bufferLength = 0x1000;
						char buffer[bufferLength];

						IFileHandle* FileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*fileName);

						while (ps.numBytesRequired > 0)
						{
							int len = FMath::Min(ps.numBytesRequired, bufferLength);
							FileHandle->Read((uint8*)&buffer, len);

							This->FrozenWorldInterop.DeserializeWrite(&ps, len, &buffer[0]);
						}

						This->FrozenWorldInterop.DeserializeApply(&ps);
						This->FrozenWorldInterop.DeserializeClose(&ps);

						This->FrozenWorldAnchorManager.LoadAnchors();
						This->FrozenWorldAlignmentManager.Load();

						// finish when reading was successful
						delete FileHandle;
						FileHandle = nullptr;
						break;
					}
				}

				This->hasPendingLoadTask = false;
				This->initializationState = InitializationState::Running;
			}
		});
	}

	void FFrozenWorldPlugin::SaveAsync()
	{
		if (HasPendingIO())
		{
			return;
		}

		TSharedPtr<BackgroundOperation> SaveTask = MakeShared<BackgroundOperation>();
		SaveTask->QueueBackgroundTask([WeakThis{ TWeakPtr<FFrozenWorldPlugin, ESPMode::ThreadSafe>(AsShared()) }, SaveTask]()
		{
			if (TSharedPtr<FFrozenWorldPlugin, ESPMode::ThreadSafe> This = WeakThis.Pin())
			{
				This->hasPendingSaveTask = true;

				FString filePath = This->stateFileNameBase;
				FString newFilePath = This->stateFileNameBase + ".new";
				if (FPaths::FileExists(newFilePath))
				{
					FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*newFilePath);
				}

				This->FrozenWorldAlignmentManager.Save();

				FrozenWorld_Serialize_Stream ps;
				ps.includePersistent = true;
				ps.includeTransient = false;
				This->FrozenWorldInterop.SerializeOpen(&ps);
				This->FrozenWorldInterop.SerializeGather(&ps);

				TUniquePtr<IFileHandle> FileHandle;
				FileHandle.Reset(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*filePath));

				const int bufferLength = 0x1000;
				char buffer[bufferLength];
				while (ps.numBytesBuffered > 0)
				{
					int numBytesRead = This->FrozenWorldInterop.SerializeRead(&ps, bufferLength, &buffer[0]);
					FileHandle->Write((const uint8*)buffer, numBytesRead);
				}

				This->FrozenWorldInterop.SerializeClose(&ps);

				FString oldFilePath = This->stateFileNameBase + ".old";
				if (FPaths::FileExists(oldFilePath))
				{
					FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*oldFilePath);
				}
				if (FPaths::FileExists(This->stateFileNameBase))
				{
					FPlatformFileManager::Get().GetPlatformFile().MoveFile(*filePath, *oldFilePath);
				}
				FPlatformFileManager::Get().GetPlatformFile().MoveFile(*newFilePath, *filePath);

				This->LastSavingTime = GWorld->RealTimeSeconds;

				This->hasPendingSaveTask = false;
			}
		});
	}

	TArray<FrozenWorld_AnchorId> FFrozenWorldPlugin::GetFrozenAnchorIds()
	{
		return FrozenWorldInterop.GetFrozenAnchorIds();
	}

	void FFrozenWorldPlugin::ClearSpongyAnchors()
	{
		FrozenWorldInterop.ClearSpongyAnchors();
	}

	void FFrozenWorldPlugin::ClearFrozenAnchors()
	{
		FrozenWorldInterop.ClearFrozenAnchors();
	}

	void FFrozenWorldPlugin::Step_Init(FTransform spongyHeadPose)
	{
		FrozenWorldInterop.Step_Init(spongyHeadPose);
	}

	void FFrozenWorldPlugin::AddSpongyAnchors(TArray<FrozenWorld_Anchor> anchors)
	{
		FrozenWorldInterop.AddSpongyAnchors(anchors);
	}

	void FFrozenWorldPlugin::SetMostSignificantSpongyAnchorId(FrozenWorld_AnchorId anchorId)
	{
		FrozenWorldInterop.SetMostSignificantSpongyAnchorId(anchorId);
	}

	void FFrozenWorldPlugin::AddSpongyEdges(TArray<FrozenWorld_Edge> edges)
	{
		FrozenWorldInterop.AddSpongyEdges(edges);
	}

	void FFrozenWorldPlugin::Step_Finish()
	{
		FrozenWorldInterop.Step_Finish();
	}

	FrozenWorld_Metrics FFrozenWorldPlugin::GetMetrics()
	{
		return FrozenWorldInterop.GetMetrics();
	}

	void FFrozenWorldPlugin::RemoveFrozenAnchor(FrozenWorld_AnchorId anchorId)
	{
		FrozenWorldInterop.RemoveFrozenAnchor(anchorId);
	}

	FrozenWorld_FragmentId FFrozenWorldPlugin::GetMostSignificantFragmentId()
	{
		return FrozenWorldInterop.GetMostSignificantFragmentId();
	}

	void FFrozenWorldPlugin::CreateAttachmentPointFromHead(FVector frozenPosition, FrozenWorld_AnchorId& outAnchorId, FVector outLocationFromAnchor)
	{
		FrozenWorldInterop.CreateAttachmentPointFromHead(frozenPosition, outAnchorId, outLocationFromAnchor);
	}

	void FFrozenWorldPlugin::CreateAttachmentPointFromSpawner(FrozenWorld_AnchorId contextAnchorId, FVector contextLocationFromAnchor, FVector frozenPosition,
		FrozenWorld_AnchorId& outAnchorId, FVector& outLocationFromAnchor)
	{
		FrozenWorldInterop.CreateAttachmentPointFromSpawner(contextAnchorId, contextLocationFromAnchor,
			frozenPosition, outAnchorId, outLocationFromAnchor);
	}

	bool FFrozenWorldPlugin::ComputeAttachmentPointAdjustment(FrozenWorld_AnchorId oldAnchorId, FVector oldLocationFromAnchor,
		FrozenWorld_AnchorId& outNewAnchorId, FVector& outNewLocationFromAnchor, FTransform& outAdjustment)
	{
		return FrozenWorldInterop.ComputeAttachmentPointAdjustment(oldAnchorId, oldLocationFromAnchor,
			outNewAnchorId, outNewLocationFromAnchor, outAdjustment);
	}

	bool FFrozenWorldPlugin::Merge(FrozenWorld_FragmentId& outTargetFragment, TArray<FragmentPose> outMergedFragments)
	{
		return FrozenWorldInterop.Merge(outTargetFragment, outMergedFragments);
	}

	bool FFrozenWorldPlugin::Refreeze(FrozenWorld_FragmentId& outMergedId, TArray<FrozenWorld_FragmentId> outAbsorbedFragments)
	{
		return FrozenWorldInterop.Refreeze(outMergedId, outAbsorbedFragments);
	}

	void FFrozenWorldPlugin::RefreezeFinish()
	{
		FrozenWorldInterop.RefreezeFinish();
	}
}

