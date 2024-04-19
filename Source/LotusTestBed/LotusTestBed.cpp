// Copyright Epic Games, Inc. All Rights Reserved.

#include "LotusTestBed.h"
#include "Modules/ModuleManager.h"

#include "Misc/Paths.h"
#include "ShaderCore.h"

void FThisLotusTestBedModule::StartupModule()
{
	// Find the static location of the shader folder on your computer
	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));


	////////////////////////
	// Define the name you will use in the engine to access this folder
	//
	// FORMALISM SUGGESTION: 
	// 
	// linking from a project: TEXT("/Project/YourProjectName")
	// linking from a plugin: TEXT("/Plugin/YourPluginName")

	//AddShaderSourceDirectoryMapping(TEXT("/Lotus"), ShaderDirectory);

	///////////////////////


}

void FThisLotusTestBedModule::ShutdownModule()
{
	//Users reported it might solve linking issues - haven't had the need myself
	ResetAllShaderSourceDirectoryMappings();
}

IMPLEMENT_PRIMARY_GAME_MODULE(FThisLotusTestBedModule, LotusTestBed, "LotusTestBed")
