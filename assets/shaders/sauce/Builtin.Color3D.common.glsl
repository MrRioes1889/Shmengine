#extension GL_EXT_scalar_block_layout : enable

// Data Transfer Object
struct Dto
{
	vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUBO 
{
    mat4 projection;
	mat4 view;
} global_ubo;

layout(push_constant) uniform PushConstants 
{
	// Only guaranteed a total of 128 bytes.
	mat4 model; // 64 bytes
} local_ubo;
