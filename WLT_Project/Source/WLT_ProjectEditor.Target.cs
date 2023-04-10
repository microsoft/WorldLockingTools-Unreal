// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class WLT_ProjectEditorTarget : TargetRules
{
	public WLT_ProjectEditorTarget(TargetInfo Target) : base(Target)
	{
		// Set the target type to Editor
		Type = TargetType.Editor;
		
		// Use BuildSettingsVersion V2
		DefaultBuildSettings = BuildSettingsVersion.V2;

		// Add the name of the project module to ExtraModuleNames list
		ExtraModuleNames.AddRange( new string[] { "WLT_Project" } );
	}
}
