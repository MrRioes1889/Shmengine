#version 450

layout(location = 0) flat in int in_mode;
layout(location = 1) in struct dto
{
    vec2 tex_coord;
} in_dto;

layout(location = 0) out vec4 out_color;

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

void main()
{
    out_color = instance_ubo.properties.diffuse_color * texture(samplers[SAMP_DIFFUSE], in_dto.tex_coord);
}