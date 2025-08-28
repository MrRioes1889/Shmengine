#version 450
#include "Builtin.UI.common.glsl"

layout(location = 0) flat in int in_mode;
layout(location = 1) in Dto in_dto;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = instance_ubo.properties.diffuse_color * texture(samplers[SAMP_DIFFUSE], in_dto.tex_coord);
}