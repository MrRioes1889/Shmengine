#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;

layout(set = 0, binding = 0) uniform global_uniform_object
{
    mat4 projection;
    mat4 view;
    vec4 ambient_color;
} global_ubo;

layout(push_constant) uniform push_constants
{
    mat4 model;
} u_push_constants;

layout(location = 0) out int out_mode;
layout(location = 1) out struct dto
{
    vec4 ambient_color;
    vec3 normal;
    vec2 texcoord;
} out_dto;

void main()
{
    out_mode = 0;
    out_dto.normal = normalize(mat3(u_push_constants.model) * in_normal);
    out_dto.texcoord = in_texcoord;
    out_dto.ambient_color = global_ubo.ambient_color;

    gl_Position = global_ubo.projection * global_ubo.view * u_push_constants.model * vec4(in_position, 1.0);
}