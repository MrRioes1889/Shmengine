#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec2 in_position;

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

// Data Transfer Object
layout(location = 1) out struct DTO 
{
	vec4 id_color;
} out_dto;

void main()
{
    out_dto.id_color = vec4(u_push_constants.id_color, 1.0);
    gl_Position = global_ubo.projection * global_ubo.view * u_push_constants.model * vec4(in_position, 0.0, 1.0);
}