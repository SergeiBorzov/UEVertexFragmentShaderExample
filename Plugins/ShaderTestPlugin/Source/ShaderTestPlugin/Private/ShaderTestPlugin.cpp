// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderTestPlugin.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FShaderTestPluginModule"

void FShaderTestPluginModule::StartupModule()
{
	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ShaderTestPlugin/Shaders"));
	AddShaderSourceDirectoryMapping("/CustomShaders", ShaderDirectory);
}

void FShaderTestPluginModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FShaderTestPluginModule, ShaderTestPlugin)