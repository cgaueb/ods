// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO; // Needed for 'Path'

public class LotusTestBed : ModuleRules
{
	public LotusTestBed(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);
		string BayesOptPath = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty");
		string BoostPath = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty");

		PrivateIncludePaths.AddRange(
			new string[] {
				EnginePath + "Source/Runtime/Renderer/Private",
				Path.Combine(BayesOptPath, "BayesOpt", "include"),
                Path.Combine(BayesOptPath, "BayesOpt", "utils")
				// ... add other private include paths required here ...
			}
		);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore",
			"Projects", // Needed for IPluginManager
			"Core",
			"CoreUObject",
			"Engine",
			"Renderer",
			"RenderCore",
			"RHI",
			"GeometryFramework",
            "GeometryScriptingCore",
            "GeometryScriptingEditor",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { 
			"Projects",
			"Core",
			"CoreUObject",
			"Engine",
			"Renderer",
			"RenderCore",
			"RHI",
		});

		PublicDelayLoadDLLs.AddRange(new string[] { "bayesopt.dll" });

		PublicAdditionalLibraries.AddRange(new string[] {
			Path.Combine(BayesOptPath, "BayesOpt", "lib", "Release", "BayesOpt.lib"),
			Path.Combine(BayesOptPath, "BayesOpt", "lib", "Debug", "BayesOpt.lib"),});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true

		//CppStandard = CppStandardVersion.Cpp20;
	}
}
