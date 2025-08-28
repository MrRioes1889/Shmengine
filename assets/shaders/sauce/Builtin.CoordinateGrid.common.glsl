#extension GL_EXT_scalar_block_layout : enable

// Data Transfer Object
struct Dto
{
	vec3 near_point;
    vec3 far_point;
};

layout(set = 0, binding = 0) uniform global_uniform_object
{
    mat4 projection;
    mat4 view;
    float near;
    float far;
} global_ubo;