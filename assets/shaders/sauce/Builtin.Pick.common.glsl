#extension GL_EXT_scalar_block_layout : enable

struct Dto 
{
	vec4 id_color;
};

layout(set = 0, binding = 0) uniform global_uniform_object
{
    mat4 projection;
    mat4 view;
} global_ubo;

layout(push_constant) uniform push_constants
{
    mat4 model;
    vec3 id_color;
} u_push_constants;
