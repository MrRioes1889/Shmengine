#extension GL_EXT_scalar_block_layout : enable

struct Dto
{
    vec2 tex_coord;
};

layout(set = 0, binding = 0) uniform global_uniform_object
{
    mat4 projection;
    mat4 view;
} global_ubo;

struct UIMaterialProperties
{
    vec4 diffuse_color;
};

layout(set = 1, binding = 0) uniform instance_uniform_object
{
    UIMaterialProperties properties;
} instance_ubo;

const int SAMP_DIFFUSE = 0;
layout(set = 1, binding = 1) uniform sampler2D samplers[1];

layout(push_constant) uniform push_constants
{
    mat4 model;
} u_push_constants;