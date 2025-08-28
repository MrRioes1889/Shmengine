#version 450
#include "Builtin.Pick.common.glsl"

layout(location = 0) in vec3 in_position;

layout(location = 0) out Dto out_dto;

void main()
{
    out_dto.id_color = vec4(u_push_constants.id_color, 1.0);
    gl_Position = global_ubo.projection * global_ubo.view * u_push_constants.model * vec4(in_position, 1.0);
}