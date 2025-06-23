#pragma once

#include "Defines.hpp"

enum class ResourceState : uint8
{
	Uninitialized,
	Destroyed,
	Initializing,
	Initialized,
	Loading,
	Loaded,
	Unloading,
	Unloaded
};