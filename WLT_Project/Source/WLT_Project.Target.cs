// Copyright Notice: Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class WLT_ProjectTarget : TargetRules
{
	// Constructor for WLT_ProjectTarget
	public WLT_ProjectTarget(TargetInfo Target) : base(Target)
	{
		// Set the target type to game
		Type = TargetType.Game;

		// Set the default build settings version to V2
		DefaultBuildSettings = BuildSettingsVersion.V2;

		// Add the WLT_Project module to the list of extra module names
		ExtraModuleNames.AddRange( new string[] { "WLT_Project" } );
	}
}
