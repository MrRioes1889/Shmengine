#extension GL_EXT_scalar_block_layout : enable

layout(set = 0, binding = 0) uniform global_uniform_object {
    mat4 projection;
	mat4 view;
} global_ubo;

// Samplers
const int SAMP_DIFFUSE = 0;
layout(set = 1, binding = 0) uniform samplerCube samplers[1];