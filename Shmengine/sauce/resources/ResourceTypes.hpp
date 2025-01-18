#pragma once

#include "Defines.hpp"

enum class ResourceState : uint8
{
	UNINITIALIZED,
	DESTROYED,
	INITIALIZING,
	INITIALIZED,
	LOADING,
	LOADED,
	UNLOADING,
	UNLOADED
};