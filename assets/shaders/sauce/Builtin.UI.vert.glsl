#version 450
#include "Builtin.UI.common.glsl"

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_tex_coord;

layout(location = 0) out int out_mode;
layout(location = 1) out Dto out_dto;

void main()
{
    out_mode = 0;
    out_dto.tex_coord = vec2(in_tex_coord.x, 1.0 - in_tex_coord.y);
    gl_Position = global_ubo.projection * global_ubo.view * u_push_constants.model * vec4(in_position, 0.0, 1.0);
}