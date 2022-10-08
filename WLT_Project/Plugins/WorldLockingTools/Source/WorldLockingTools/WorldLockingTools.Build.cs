// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnrealBuildTool;

public class WorldLockingTools : ModuleRules
{
	public WorldLockingTools(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Platform == UnrealTargetPlatform.Win64 ||
			// FrozenWorld NuGet package does not have x64 UWP binaries.
			(Target.Platform == UnrealTargetPlatform.HoloLens && Target.WindowsPlatform.Architecture.ToString() != "x64"))
		{
			PublicDefinitions.Add("USING_FROZEN_WORLD");

			// these parameters mandatory for winrt support
			bEnableExceptions = true;
			bUseUnity = false;
			CppStandard = CppStandardVersion.Cpp17;
			PublicSystemLibraries.AddRange(new string[] { "shlwapi.lib", "runtimeobject.lib" });

			// prepare everything for nuget
			string MyModuleName = GetType().Name;
			string NugetFolder = Path.Combine(PluginDirectory, "Intermediate", "Nuget", MyModuleName);
			Directory.CreateDirectory(NugetFolder);

			string BinariesSubFolder = Path.Combine("Binaries", "ThirdParty", Target.Type.ToString(), Target.Platform.ToString(), Target.Architecture);

			PublicDefinitions.Add(string.Format("THIRDPARTY_BINARY_SUBFOLDER=\"{0}\"", BinariesSubFolder.Replace(@"\", @"\\")));

			string BinariesFolder = Path.Combine(PluginDirectory, BinariesSubFolder);
			Directory.CreateDirectory(BinariesFolder);

			ExternalDependencies.Add("packages.config");

			// download nuget
			string NugetExe = Path.Combine(NugetFolder, "nuget.exe");
			if (!File.Exists(NugetExe))
			{
				// The System.Net assembly is not referenced by the build tool so it must be loaded dynamically.
				var assembly = System.Reflection.Assembly.Load("System.Net");
				var webClient = assembly.CreateInstance("System.Net.WebClient");
				using ((IDisposable)webClient)
				{
					// we aren't focusing on a specific nuget version, we can use any of them but the latest one is preferable
					var downloadFileMethod = webClient.GetType().GetMethod("DownloadFile", new Type[] { typeof(string), typeof(string) });
					downloadFileMethod.Invoke(webClient, new object[] { @"https://dist.nuget.org/win-x86-commandline/latest/nuget.exe", NugetExe });
				}
			}

			// run nuget to update the packages
			{
				var StartInfo = new System.Diagnostics.ProcessStartInfo(NugetExe, string.Format("install \"{0}\" -OutputDirectory \"{1}\"", Path.Combine(ModuleDirectory, "packages.config"), NugetFolder));
				StartInfo.UseShellExecute = false;
				StartInfo.CreateNoWindow = true;
				var ExitCode = Utils.RunLocalProcessAndPrintfOutput(StartInfo);
				if (ExitCode < 0)
				{
					throw new BuildException("Failed to get nuget packages.  See log for details.");
				}
			}

			// get list of the installed packages, that's needed because the code should get particular versions of the installed packages
			string[] InstalledPackages = Utils.RunLocalProcessAndReturnStdOut(NugetExe, string.Format("list -Source \"{0}\"", NugetFolder)).Split(new char[] { '\r', '\n' });

			// winmd files of the packages
			List<string> WinMDFiles = new List<string>();

			string FWPackage = InstalledPackages.FirstOrDefault(x => x.StartsWith("Microsoft.MixedReality.Unity.FrozenWorld.Engine"));
			if (!string.IsNullOrEmpty(FWPackage))
			{
				string FWFolderName = FWPackage.Replace(" ", ".");

				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					SafeCopy(Path.Combine(NugetFolder, FWFolderName, string.Format(@"lib\unity\Windows-{0}\FrozenWorldPlugin.dll", Target.WindowsPlatform.Architecture.ToString())),
						Path.Combine(BinariesFolder, "FrozenWorldPlugin.dll"));
				}
				else
				{
					SafeCopy(Path.Combine(NugetFolder, FWFolderName, string.Format(@"lib\unity\Windows-UWP-{0}\FrozenWorldPlugin.dll", Target.WindowsPlatform.Architecture.ToString())),
						Path.Combine(BinariesFolder, "FrozenWorldPlugin.dll"));
				}

				RuntimeDependencies.Add(Path.Combine(BinariesFolder, "FrozenWorldPlugin.dll"));
			}
			else
			{
				Log.TraceError("Failed to find the Microsoft.MixedReality.Unity.FrozenWorld.Engine package.  Check the packages.config file.");
			}
		}

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HeadMountedDisplay",
				"AugmentedReality",
				"Projects",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd"
				}
			);
		}
	}

	private void SafeCopy(string source, string destination)
	{
		if (!File.Exists(source))
		{
			Log.TraceError("Class {0} can't find {1} file for copying", this.GetType().Name, source);
			return;
		}

		try
		{
			File.Copy(source, destination, true);
		}
		catch (IOException ex)
		{
			Log.TraceWarning("Failed to copy {0} to {1} with exception: {2}", source, destination, ex.Message);
			if (!File.Exists(destination))
			{
				Log.TraceError("Destination file {0} does not exist", destination);
				return;
			}

			Log.TraceWarning("Destination file {0} already existed and is probably in use.  The old file will be used for the runtime dependency.  This may happen when packaging a Win64 exe from the editor.", destination);
		}
	}
}
