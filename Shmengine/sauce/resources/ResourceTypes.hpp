#pragma once

#include "Defines.hpp"

enum class ResourceState : uint8
{
	Uninitialized,
	Destroyed,
	Initialized,
	Initializing,
	Destroying,
};