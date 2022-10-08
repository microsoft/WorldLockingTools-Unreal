// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "AlignmentManager.h"

#include "FrozenWorldPoseExtensions.h"
#include "FrozenWorldPlugin.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

namespace WorldLockingTools
{
	FSimpleMulticastDelegate FAlignmentManager::OnAlignmentManagerLoad;
	FSimpleMulticastDelegate FAlignmentManager::OnAlignmentManagerReset;

	/// <summary>
	/// When the reference point position is initially set, create an attachment point if there isn't one,
	/// or if there is, updated its position.
	/// </summary>
	void FReferencePose::CheckAttachmentPoint()
	{
		if (AttachmentPoint == nullptr)
		{
			LocationHandler.BindRaw(this, &FReferencePose::OnLocationUpdate);

			AttachmentPoint = FFragmentManager::Get()->CreateAttachmentPoint(lockedPose.GetLocation(), nullptr, LocationHandler, nullptr);
		}
		else
		{
			FFragmentManager::Get()->TeleportAttachmentPoint(AttachmentPoint, lockedPose.GetLocation(), nullptr);
		}
	}

	/// <summary>
	/// Update the pose for refit operations.
	/// </summary>
	/// <param name="adjustment">The adjustment to apply.</param>
	void FReferencePose::OnLocationUpdate(FTransform adjustment)
	{
		fragmentId = FFragmentManager::Get()->GetCurrentFragmentId();
		lockedPose = FFrozenWorldPoseExtensions::Multiply(lockedPose, adjustment);
		AfterAdjustmentPoseChanged();
	}

	FAlignmentManager* FAlignmentManager::Get()
	{
		return &IModularFeatures::Get().GetModularFeature<FAlignmentManager>(GetModularFeatureName());
	}

	FAlignmentManager::FAlignmentManager()
	{
		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
	}

	FAlignmentManager::~FAlignmentManager()
	{
		IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
	}

	// Do the weighted average of all active reference poses to get an alignment pose.
	void FAlignmentManager::ComputePinnedPose(FTransform lockedHeadPose)
	{
		CheckSend();
		CheckFragment();
		CheckSave();
		if (activePoses.Num() < 1)
		{
			PinnedFromLocked = FTransform::Identity;
		}
		else
		{
			TArray<WeightedPose> poses = ComputePoseWeights(lockedHeadPose.GetLocation());
			PinnedFromLocked = WeightedAverage(poses);
		}
	}

	TArray<WeightedPose> FAlignmentManager::ComputePoseWeights(FVector lockedHeadPosition)
	{
		weightedPoses.Empty();

		Interpolant bary;
		if (triangulator.Find(lockedHeadPosition, bary))
		{
			for (int i = 0; i < 3; ++i)
			{
				weightedPoses.Add(
					WeightedPose
					{
						ComputePinnedFromLocked(activePoses[bary.idx[i]]),
						bary.weights[i]
					}
				);
			}
		}
		else
		{
			check(activePoses.Num() == 0); //Failed to find an interpolant even though there are pins active.
		}

		return weightedPoses;
	}

	/// <summary>
	/// Compute the PinnedFromLocked pose for the given reference pose.
	/// </summary>
	/// <param name="refPose">The reference pose to evaluate.</param>
	/// <returns>The computed PinnedFromLocked pose.</returns>
	FTransform FAlignmentManager::ComputePinnedFromLocked(FReferencePose refPose)
	{
		FTransform pinnedFromObject = refPose.virtualPose;
		FTransform objectFromLocked = FFrozenWorldPoseExtensions::Inverse(refPose.LockedPose());

		return FFrozenWorldPoseExtensions::Multiply(pinnedFromObject, objectFromLocked);
	}

	
	/// <summary>
	/// Collapse a list of weighted poses into a single equivalent pose.
	/// </summary>
	/// <param name="poses">The poses to average.</param>
	/// <returns>The weighted average. If the list is empty, returns an identity pose.</returns>
	FTransform FAlignmentManager::WeightedAverage(TArray<WeightedPose> poses)
	{
		if (poses.Num() < 1)
		{
			return FTransform::Identity;
		}

		while (poses.Num() > 1)
		{
			// Collapse the last two into an equivalent average one.
			poses[poses.Num() - 2] = WeightedAverage(poses[poses.Num() - 2], poses[poses.Num() - 1]);
			poses.RemoveAt(poses.Num() - 1);
		}

		return poses[0].pose;
	}
	/// <summary>
	/// Combine two weighted poses via interpolation into a single equivalent weighted pose.
	/// </summary>
	/// <param name="lhs">Left hand pose</param>
	/// <param name="rhs">Right hand pose</param>
	/// <returns>The equivalent pose.</returns>
	WeightedPose FAlignmentManager::WeightedAverage(WeightedPose lhs, WeightedPose rhs)
	{
		float minCombinedWeight = 0.0f;
		if (lhs.weight + rhs.weight <= minCombinedWeight)
		{
			return WeightedPose
			{
				FTransform::Identity,
				0.0f
			};
		}
		float interp = rhs.weight / (lhs.weight + rhs.weight);

		WeightedPose ret;
		FVector pos = lhs.pose.GetLocation() + interp * (rhs.pose.GetLocation() - lhs.pose.GetLocation());
		FQuat rot = FQuat::Slerp(lhs.pose.GetRotation(), rhs.pose.GetRotation(), interp);
		rot.Normalize();
		ret.pose = FTransform(rot, pos);
		ret.weight = lhs.weight + rhs.weight;

		return ret;
	}

	/// <summary>
	/// Complete any queued saves.
	/// </summary>
	void FAlignmentManager::CheckSave()
	{
		if (referencePosesToSave.Num() > 0)
		{
			//DebugLogSaveLoad($"{SaveFileName} has {referencePosesToSave.Count} to save");
			for (int i = referencePosesToSave.Num() - 1; i >= 0; --i)
			{
				poseDB.Set(referencePosesToSave[i]);
			}
			referencePosesToSave.Empty();
			needSave = true;
		}
	}

	/// <summary>
	/// If any reference poses are eligible, promote them to active.
	/// </summary>
	void FAlignmentManager::CheckSend()
	{
		if (needSend)
		{
			PerformSendAlignmentAnchors();
			needSend = false;
		}
	}

	/// <summary>
	/// Actually perform the send of the pending new list of alignment anchors into the active state.
	/// 
	/// This is deferred after request until update, to be sure all the pieces have been 
	/// wired up during startup.
	/// </summary>
	void FAlignmentManager::PerformSendAlignmentAnchors()
	{
		sentPoses.Empty();
		for (int i = 0; i < referencePoses.Num(); ++i)
		{
			sentPoses.Add(referencePoses[i]);
		}
		ActivateCurrentFragment();
	}

	void FAlignmentManager::ActivateCurrentFragment()
	{
		//DebugLogSaveLoad($"Active fragment from {ActiveFragmentId.FormatStr()} to {CurrentFragmentId.FormatStr()}");
		activePoses.Empty();
		for (int i = 0; i < sentPoses.Num(); ++i)
		{
			if (sentPoses[i].IsActive(FFragmentManager::Get()->GetCurrentFragmentId()))
			{
				activePoses.Add(sentPoses[i]);
			}
		}
		activeFragmentId = FFragmentManager::Get()->GetCurrentFragmentId();
		BuildTriangulation();
	}

	void FAlignmentManager::BuildTriangulation()
	{
		// Seed with for far-out corners
		InitTriangulator();
		if (activePoses.Num() > 0)
		{
			TArray<FVector> positions;
			for (int i = 0; i < activePoses.Num(); ++i)
			{
				positions.Add(activePoses[i].LockedPose().GetLocation());
			}
			triangulator.Add(positions);
		}
	}

	void FAlignmentManager::InitTriangulator()
	{
		triangulator.Clear();
		if (activePoses.Num() > 0)
		{
			triangulator.SetBounds(FVector(-100000, -100000, 0), FVector(100000, 100000, 0));
		}
	}

	/// <summary>
	/// If still waiting for a valid current fragment since last load,
	/// and there is a current valid fragment, set it to reference poses.
	/// </summary>
	void FAlignmentManager::CheckFragment()
	{
		FrozenWorld_FragmentId CurrentFragmentId = FFragmentManager::Get()->GetCurrentFragmentId();
		bool changed = activeFragmentId != CurrentFragmentId;
		if (needFragment 
			&& CurrentFragmentId != FrozenWorld_FragmentId_INVALID 
			&& CurrentFragmentId != FrozenWorld_FragmentId_UNKNOWN)
		{
			FrozenWorld_FragmentId fragmentId = CurrentFragmentId;
			for (int i = 0; i < referencePoses.Num(); ++i)
			{
				if (referencePoses[i].fragmentId == FrozenWorld_FragmentId_INVALID 
					|| referencePoses[i].fragmentId == FrozenWorld_FragmentId_UNKNOWN)
				{
					//DebugLogSaveLoad($"Transfer {referencePoses[i].anchorId.FormatStr()} from frag={referencePoses[i].fragmentId.FormatStr()} to {fragmentId.FormatStr()}");
					referencePoses[i].fragmentId = fragmentId;
					changed = true;
				}
			}
			needFragment = false;
		}
		if (changed)
		{
			ActivateCurrentFragment();
		}
	}

	/// <summary>
	/// Remove all alignment anchors that have been added. More efficient than removing them individually, 
	/// and doesn't require having stored their ids on creation.
	/// 
	/// This is more efficient than removing one by one, but take care to discard all existing AnchorIds returned by AddAlignmentAnchor
	/// after this call, as it will be an error to try to use any of them.
	/// Also note that this clears the Alignment Anchors staged for commit with the next SendAlignmentAnchors,
	/// but the current ones will remain effective until the next call to SendAlignmentAnchors, which will send an empty list,
	/// unless it has been repopulated after the call to ClearAlignmentAnchors.
	/// </summary>
	void FAlignmentManager::ClearAlignmentAnchors()
	{
		poseDB.Empty();
		referencePoses.Empty();
		referencePosesToSave.Empty();

		FAlignmentManager::OnAlignmentManagerReset.Broadcast();
	}

	/// <summary>
	/// Submit all accumulated alignment anchors. 
	/// 
	/// All anchors previously submitted via SendAlignmentAnchors() will be cleared and replaced by the current set.
	/// SendAlignmentAnchors() submits the current set of anchors, but they will have no effect until the next 
	/// FragmentManager::Refreeze() is successfully performed.
	/// </summary>
	void FAlignmentManager::SendAlignmentAnchors()
	{
		needSend = true;
	}

	/// <summary>
	/// Add an anchor for aligning a virtual pose to a pose in real space. 
	/// 
	/// This must be followed by SendAlignmentAnchors before it will have any effect.
	/// The returned AnchorId may be stored for future manipulation of the created anchor (e.g. for individual removal in RemoveAlignmentAnchor(AnchorId)).
	/// The system must be currently tracking to successfully add an alignment anchor. The alignment anchor will be in the current Fragment.
	/// The current fragment will be available when there is no tracking, and so this call will fail. 
	/// If this call fails, indicated by a return of AnchorId_Unknown, then it should be called again on a later frame until it succeeds.
	/// </summary>
	/// <param name="virtualPose">The pose in modeling space.</param>
	/// <param name="lockedPose">The pose in world locked space.</param>
	/// <returns>The id for the added anchor if successful, else AnchorId.Unknown. See remarks.</returns>
	FrozenWorld_AnchorId FAlignmentManager::AddAlignmentAnchor(FString uniqueName, FTransform virtualPose, FTransform lockedPose)
	{
		FrozenWorld_FragmentId fragmentId = FFragmentManager::Get()->GetCurrentFragmentId();
		FrozenWorld_AnchorId anchorId = ClaimAnchorId();
		virtualPose = FFrozenWorldPoseExtensions::Multiply(FFrozenWorldPlugin::Get()->PinnedFromFrozen(), virtualPose);

		FReferencePose refPose;
		refPose.name = uniqueName;
		refPose.fragmentId = fragmentId;
		refPose.anchorId = anchorId;
		refPose.virtualPose = virtualPose;
		refPose.SetLockedPose(lockedPose);

		referencePoses.Add(refPose);
		QueueForSave(refPose);

		return anchorId;
	}

	/// <summary>
	/// Get the world locked space pose associated with this alignment anchor.
	/// </summary>
	/// <param name="anchorId">Which anchor.</param>
	/// <param name="lockedPose">Pose to fill out if alignment anchor is found.</param>
	/// <returns>True if anchor is found and lockedPose filled in, else false and lockedPose set to identity.</returns>
	bool FAlignmentManager::GetAlignmentPose(FrozenWorld_AnchorId AnchorID, FTransform& outLockedPose)
	{
		if (AnchorID != FrozenWorld_AnchorId_UNKNOWN
			&& AnchorID != FrozenWorld_AnchorId_INVALID)
		{
			for (int i = 0; i < referencePoses.Num(); ++i)
			{
				if (referencePoses[i].anchorId == AnchorID)
				{
					outLockedPose = referencePoses[i].LockedPose();
					return true;
				}
			}
		}
		outLockedPose = FTransform::Identity;
		return false;
	}

	/// <summary>
	/// Remove the given alignment anchor from the system.
	/// </summary>
	/// <param name="anchorId">The anchor to remove (as returned by AddAlignmentAnchor</param>
	/// <returns>True if the anchor was found.</returns>
	bool FAlignmentManager::RemoveAlignmentAnchor(FrozenWorld_AnchorId AnchorID)
	{
		bool found = false;
		if (AnchorID != FrozenWorld_AnchorId_UNKNOWN
			&& AnchorID != FrozenWorld_AnchorId_INVALID)
		{
			for (int i = referencePoses.Num() - 1; i >= 0; --i)
			{
				if (referencePoses[i].anchorId == AnchorID)
				{
					poseDB.Forget(referencePoses[i].name);
					referencePoses.RemoveAt(i);
					found = true;
				}
			}
			for (int i = referencePosesToSave.Num() - 1; i >= 0; --i)
			{
				if (referencePosesToSave[i].anchorId == AnchorID)
				{
					referencePosesToSave.RemoveAt(i);
				}
			}
		}

		return found;
	}

	/// <summary>
	/// Add to queue for being saved to database next chance.
	/// </summary>
	void FAlignmentManager::QueueForSave(FReferencePose refPose)
	{
		for (int i = 0; i < referencePosesToSave.Num(); i++)
		{
			if (referencePosesToSave[i].anchorId == refPose.anchorId)
			{
				// refPose is already queued for save.
				return;
			}
		}
		
		referencePosesToSave.Add(refPose);
	}

	/// <summary>
	/// Explicitly save the database.
	/// </summary>
	/// <returns>True if successfully saved.</returns>
	bool FAlignmentManager::Save()
	{
		bool saved = poseDB.Save();
		if (saved)
		{
			needSave = false;
		}

		return saved;
	}

	/// <summary>
	/// Load the database and issue notification if loaded.
	/// </summary>
	/// <returns>True if loaded.</returns>
	bool FAlignmentManager::Load()
	{
		bool loaded = poseDB.Load();
		if (loaded)
		{
			FAlignmentManager::OnAlignmentManagerLoad.Broadcast();
			SendAlignmentAnchors();
			needSave = false;
		}
		return loaded;
	}

	/// <summary>
	/// Attempt to restore an alignment anchor from an earlier session. Stored alignment anchor
	/// must match in both uniqueName and virtual pose.
	/// 
	/// If successful, alignment anchor is added but not sent. It must be followed by a call to SendAlignmentAnchors
	/// to take effect.
	/// </summary>
	/// <param name="uniqueName">Unique name use previously to create the alignment anchor.</param>
	/// <param name="virtualPose">Virtual pose to match with stored anchor pose.</param>
	/// <returns>AnchorId of restored Alignment Anchor on success, else AnchorId.Invalid.</returns>
	FrozenWorld_AnchorId FAlignmentManager::RestoreAlignmentAnchor(FString uniqueName, FTransform virtualPose)
	{
		FReferencePose refPose;
		if (!poseDB.Get(uniqueName, refPose))
		{
			return FrozenWorld_AnchorId_INVALID;
		}
		
		bool referencePoseUpdated = false;
		for (int index = 0; index < referencePoses.Num(); index++)
		{
			if (referencePoses[index].name == uniqueName)
			{
				/// The reference pose already exists. Update it by replacing it
				/// with the new refpose using same anchor id.
				refPose.anchorId = referencePoses[index].anchorId;
				referencePoses[index] = refPose;

				referencePoseUpdated = true;
				break;
			}
		}

		if (!referencePoseUpdated)
		{
			referencePoses.Add(refPose);
		}

		/// If the referencePose has an invalid fragment id, it's only because there isn't a valid
		/// fragment right now. Flag the condition and set the proper fragment id when there is
		/// a valid one.
		if (refPose.fragmentId == FrozenWorld_FragmentId_INVALID ||
			refPose.fragmentId == FrozenWorld_FragmentId_UNKNOWN)
		{
			needFragment = true;
		}
		return refPose.anchorId;
	}

	/// <summary>
	/// Create a stream and save the database to it. Existing data is overwritten.
	/// </summary>
	/// <returns>True if successfully written.</returns>
	bool FReferencePoseDB::Save()
	{
		FString AlignmentFile = "Persistence/Alignment.fwb";
		FString FullPath = FPlatformProcess::UserDir() / AlignmentFile;
		FString Directory = FPaths::GetPath(FullPath);

		if (!FPaths::DirectoryExists(Directory))
		{
			FPlatformFileManager::Get().GetPlatformFile().CreateDirectory(*Directory);
		}

		IFileHandle* FileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*FullPath);
		FileHandle->Write((const uint8*)&version, sizeof(uint32));
		int32 dataCount = data.Num();
		FileHandle->Write((const uint8*)&dataCount, sizeof(int32));

		for (auto keyVal : data)
		{
			FString key = keyVal.Key;
			FStringView keyView = FStringView(key);
			FTCHARToUTF8 UTF8String(keyView.GetData(), keyView.Len());

			int32 keyLen = (UTF8String.Length()) * sizeof(UTF8CHAR);
			FileHandle->Write((const uint8*)&keyLen, sizeof(int32));
			FileHandle->Write((const uint8*)&UTF8String.Get()[0], keyLen);
			keyVal.Value.Write(FileHandle);
		}

		delete FileHandle;
		FileHandle = nullptr;

		return true;
	}

	/// <summary>
	/// Open a stream and load the database from it.
	/// 
	/// Reference poses are assigned to the fragment that is current at the time of load.
	/// If there is not a valid current fragment at the time of their load, they will be assigned
	/// the first valid fragment.
	/// </summary>
	/// <returns>True if the database is successfully loaded.</returns>
	bool FReferencePoseDB::Load()
	{
		FString AlignmentFile = "Persistence/Alignment.fwb";
		FString FullPath = FPlatformProcess::UserDir() / AlignmentFile;

		bool loaded = false;
		
		data.Empty();
		IFileHandle* FileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*FullPath);

		uint32 v = 0;
		FileHandle->Read((uint8*)&v, sizeof(uint32));
		if (v == version)
		{
			int32 count = 0;
			FileHandle->Read((uint8*)&count, sizeof(int32));

			for (int i = 0; i < count; ++i)
			{
				int32 nameLen = 0;
				FileHandle->Read((uint8*)&nameLen, sizeof(int32));

				TArray<UTF8CHAR> nameData;
				nameData.AddDefaulted(nameLen + 1);
				FileHandle->Read((uint8*)&nameData[0], nameLen);
				FString name = FString(UTF8_TO_TCHAR(&nameData[0]));

				Element elem = Element::Read(FileHandle);
				data.Add(name, elem);
			}

			loaded = true;
		}

		delete FileHandle;
		FileHandle = nullptr;

		IsLoaded = true;
		return loaded;
	}

	/// <summary>
	/// If the given name is represented in the database, create a corresponding reference point.
	/// </summary>
	/// <param name="uniqueName">Unique name for the reference point.</param>
	/// <returns>A valid reference point if found, else null.</returns>
	bool FReferencePoseDB::Get(FString uniqueName, FReferencePose& outRefPose)
	{
		if (!data.Contains(uniqueName))
		{
			return false;
		}

		Element src = data[uniqueName];
		outRefPose.name = uniqueName;
		outRefPose.fragmentId = FFragmentManager::Get()->GetCurrentFragmentId();
		outRefPose.anchorId = FAlignmentManager::Get()->ClaimAnchorId();
		outRefPose.virtualPose = src.virtualPose;
		outRefPose.SetLockedPose(src.lockedPose);

		return true;
	}

	/// <summary>
	/// Write the element to the given file handle. Data is appended.
	/// </summary>
	/// <param name="FileHandle">File to write the data to.</param>
	void FReferencePoseDB::Element::Write(IFileHandle* FileHandle)
	{
		double xPosVirtual = virtualPose.GetLocation().X;
		double yPosVirtual = virtualPose.GetLocation().Y;
		double zPosVirtual = virtualPose.GetLocation().Z;
		FileHandle->Write((const uint8*)&xPosVirtual, sizeof(double));
		FileHandle->Write((const uint8*)&yPosVirtual, sizeof(double));
		FileHandle->Write((const uint8*)&zPosVirtual, sizeof(double));

		double xRotVirtual = virtualPose.GetRotation().X;
		double yRotVirtual = virtualPose.GetRotation().Y;
		double zRotVirtual = virtualPose.GetRotation().Z;
		double wRotVirtual = virtualPose.GetRotation().W;
		FileHandle->Write((const uint8*)&xRotVirtual, sizeof(double));
		FileHandle->Write((const uint8*)&yRotVirtual, sizeof(double));
		FileHandle->Write((const uint8*)&zRotVirtual, sizeof(double));
		FileHandle->Write((const uint8*)&wRotVirtual, sizeof(double));

		double xPosLocked = lockedPose.GetLocation().X;
		double yPosLocked = lockedPose.GetLocation().Y;
		double zPosLocked = lockedPose.GetLocation().Z;
		FileHandle->Write((const uint8*)&xPosLocked, sizeof(double));
		FileHandle->Write((const uint8*)&yPosLocked, sizeof(double));
		FileHandle->Write((const uint8*)&zPosLocked, sizeof(double));

		double xRotLocked = lockedPose.GetRotation().X;
		double yRotLocked = lockedPose.GetRotation().Y;
		double zRotLocked = lockedPose.GetRotation().Z;
		double wRotLocked = lockedPose.GetRotation().W;
		FileHandle->Write((const uint8*)&xRotLocked, sizeof(double));
		FileHandle->Write((const uint8*)&yRotLocked, sizeof(double));
		FileHandle->Write((const uint8*)&zRotLocked, sizeof(double));
		FileHandle->Write((const uint8*)&wRotLocked, sizeof(double));
	}

	/// <summary>
	/// Read an element from the current cursor position in file.
	/// </summary>
	/// <param name="FileHandle">File to read from.</param>
	/// <returns>The element read.</returns>
	FReferencePoseDB::Element FReferencePoseDB::Element::Read(IFileHandle* FileHandle)
	{
		FReferencePoseDB::Element element;

		double xPosVirtual = 0;
		double yPosVirtual = 0;
		double zPosVirtual = 0;
		FileHandle->Read((uint8*)&xPosVirtual, sizeof(double));
		FileHandle->Read((uint8*)&yPosVirtual, sizeof(double));
		FileHandle->Read((uint8*)&zPosVirtual, sizeof(double));

		double xRotVirtual = 0;
		double yRotVirtual = 0;
		double zRotVirtual = 0;
		double wRotVirtual = 0;
		FileHandle->Read((uint8*)&xRotVirtual, sizeof(double));
		FileHandle->Read((uint8*)&yRotVirtual, sizeof(double));
		FileHandle->Read((uint8*)&zRotVirtual, sizeof(double));
		FileHandle->Read((uint8*)&wRotVirtual, sizeof(double));

		element.virtualPose = FTransform(
			FQuat(xRotVirtual, yRotVirtual, zRotVirtual, wRotVirtual),
			FVector(xPosVirtual, yPosVirtual, zPosVirtual)
		);

		double xPosLocked = 0;
		double yPosLocked = 0;
		double zPosLocked = 0;
		FileHandle->Read((uint8*)&xPosLocked, sizeof(double));
		FileHandle->Read((uint8*)&yPosLocked, sizeof(double));
		FileHandle->Read((uint8*)&zPosLocked, sizeof(double));

		double xRotLocked = 0;
		double yRotLocked = 0;
		double zRotLocked = 0;
		double wRotLocked = 0;
		FileHandle->Read((uint8*)&xRotLocked, sizeof(double));
		FileHandle->Read((uint8*)&yRotLocked, sizeof(double));
		FileHandle->Read((uint8*)&zRotLocked, sizeof(double));
		FileHandle->Read((uint8*)&wRotLocked, sizeof(double));

		element.lockedPose = FTransform(
			FQuat(xRotLocked, yRotLocked, zRotLocked, wRotLocked),
			FVector(xPosLocked, yPosLocked, zPosLocked)
		);

		return element;
	}
}