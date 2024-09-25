#pragma once

#include "utility/MathTypes.hpp"
#include "resources/ResourceTypes.hpp"

SHMAPI bool32 skybox_create(const char* cubemap_name, Skybox* out_skybox);
SHMAPI void skybox_destroy(Skybox* skybox);